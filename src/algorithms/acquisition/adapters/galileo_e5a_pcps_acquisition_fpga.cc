/*!
 * \file galileo_e5a_pcps_acquisition_fpga.cc
 * \brief Adapts a PCPS acquisition block to an AcquisitionInterface for
 *  Galileo E5a data and pilot Signals for the FPGA
 * \author Marc Majoral, 2019. mmajoral(at)cttc.es
 *
 * -----------------------------------------------------------------------------
 *
 * GNSS-SDR is a Global Navigation Satellite System software-defined receiver.
 * This file is part of GNSS-SDR.
 *
 * Copyright (C) 2010-2022  (see AUTHORS file for a list of contributors)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * -----------------------------------------------------------------------------
 */

#include "galileo_e5a_pcps_acquisition_fpga.h"
#include "Galileo_E5a.h"
#include "configuration_interface.h"
#include "galileo_e5_signal_replica.h"
#include "gnss_sdr_fft.h"
#include "gnss_sdr_flags.h"
#include <gnuradio/gr_complex.h>  // for gr_complex
#include <volk/volk.h>            // for volk_32fc_conjugate_32fc
#include <volk_gnsssdr/volk_gnsssdr_alloc.h>
#include <algorithm>  // for copy_n
#include <cmath>      // for abs, pow, floor
#include <complex>    // for complex

#if USE_GLOG_AND_GFLAGS
#include <glog/logging.h>
#else
#include <absl/log/log.h>
#endif

GalileoE5aPcpsAcquisitionFpga::GalileoE5aPcpsAcquisitionFpga(
    const ConfigurationInterface* configuration,
    const std::string& role,
    unsigned int in_streams,
    unsigned int out_streams)
    : gnss_synchro_(nullptr),
      role_(role),
      doppler_center_(0),
      channel_(0),
      in_streams_(in_streams),
      out_streams_(out_streams),
      acq_pilot_(configuration->property(role + ".acquire_pilot", false)),
      acq_iq_(configuration->property(role + ".acquire_iq", false))
{
    // Set acquisition parameters
    acq_parameters_.SetFromConfiguration(configuration, role_, DEFAULT_FPGA_BLK_EXP, GALILEO_E5A_CODE_CHIP_RATE_CPS, GALILEO_E5A_CODE_LENGTH_CHIPS);

    // Query the capabilities of the instantiated FPGA Acquisition IP Core. When the FPGA is in use, the acquisition resampler operates only in the L1/E1 frequency band.
    std::vector<std::pair<uint32_t, uint32_t>> downsampling_filter_specs;
    uint32_t max_FFT_size;
    acquisition_fpga_ = pcps_make_acquisition_fpga(&acq_parameters_, ACQ_BUFF_1, downsampling_filter_specs, max_FFT_size);

    // When the FPGA is in use, the acquisition resampler operates only in the L1/E1 frequency band.
    // Check whether the acquisition configuration is supported by the FPGA.
    bool acq_configuration_valid = acq_parameters_.Is_acq_config_valid(max_FFT_size);

    if (!acq_configuration_valid)
        {
            std::cout << "The FPGA does not support the required sampling frequency of " << acq_parameters_.fs_in << " SPS for the L5/E5a band. Please update the sampling frequency in the configuration file." << std::endl;
            exit(0);
        }

    DLOG(INFO) << "role " << role;

    generate_galileo_e5a_prn_codes();

#if USE_GLOG_AND_GFLAGS
    if (FLAGS_doppler_max != 0)
        {
            acq_parameters_.doppler_max = FLAGS_doppler_max;
        }
#else
    if (absl::GetFlag(FLAGS_doppler_max) != 0)
        {
            acq_parameters_.doppler_max = absl::GetFlag(FLAGS_doppler_max);
        }
#endif
    doppler_max_ = acq_parameters_.doppler_max;
    doppler_step_ = static_cast<unsigned int>(acq_parameters_.doppler_step);

    if (in_streams_ > 1)
        {
            LOG(ERROR) << "This implementation only supports one input stream";
        }
    if (out_streams_ > 0)
        {
            LOG(ERROR) << "This implementation does not provide an output stream";
        }
}

void GalileoE5aPcpsAcquisitionFpga::generate_galileo_e5a_prn_codes()
{
    uint32_t code_length = acq_parameters_.code_length;
    uint32_t nsamples_total = acq_parameters_.fft_size;

    // compute all the GALILEO E5 PRN Codes (this is done only once in the class constructor in order to avoid re-computing the PRN codes every time
    // a channel is assigned)
    auto fft_if = gnss_fft_fwd_make_unique(nsamples_total);  // Direct FFT
    volk_gnsssdr::vector<std::complex<float>> code(nsamples_total);
    volk_gnsssdr::vector<std::complex<float>> fft_codes_padded(nsamples_total);
    d_all_fft_codes_ = volk_gnsssdr::vector<uint32_t>(nsamples_total * GALILEO_E5A_NUMBER_OF_CODES);  // memory containing all the possible fft codes for PRN 0 to 32

    if (acq_iq_)
        {
            acq_pilot_ = false;
        }

    float max;
    int32_t tmp;
    int32_t tmp2;
    int32_t local_code;
    int32_t fft_data;

    for (uint32_t PRN = 1; PRN <= GALILEO_E5A_NUMBER_OF_CODES; PRN++)
        {
            std::array<char, 3> signal_;
            signal_[0] = '5';
            signal_[2] = '\0';

            if (acq_iq_)
                {
                    signal_[1] = 'X';
                }
            else if (acq_pilot_)
                {
                    signal_[1] = 'Q';
                }
            else
                {
                    signal_[1] = 'I';
                }

            galileo_e5_a_code_gen_complex_sampled(code, PRN, signal_, acq_parameters_.fs_in, 0);

            if (acq_parameters_.enable_zero_padding)
                {
                    // Duplicate the code sequence
                    std::copy(code.begin(), code.begin() + code_length, code.begin() + code_length);
                }

            // Fill in zero padding for the rest
            std::fill(code.begin() + (acq_parameters_.enable_zero_padding ? 2 * code_length : code_length), code.end(), std::complex<float>(0.0, 0.0));

            std::copy_n(code.data(), nsamples_total, fft_if->get_inbuf());                            // copy to FFT buffer
            fft_if->execute();                                                                        // Run the FFT of local code
            volk_32fc_conjugate_32fc(fft_codes_padded.data(), fft_if->get_outbuf(), nsamples_total);  // conjugate values

            max = 0;                                       // initialize maximum value
            for (uint32_t i = 0; i < nsamples_total; i++)  // search for maxima
                {
                    if (std::abs(fft_codes_padded[i].real()) > max)
                        {
                            max = std::abs(fft_codes_padded[i].real());
                        }
                    if (std::abs(fft_codes_padded[i].imag()) > max)
                        {
                            max = std::abs(fft_codes_padded[i].imag());
                        }
                }
            // map the FFT to the dynamic range of the fixed point values an copy to buffer containing all FFTs
            // and package codes in a format that is ready to be written to the FPGA
            for (uint32_t i = 0; i < nsamples_total; i++)
                {
                    tmp = static_cast<int32_t>(floor(fft_codes_padded[i].real() * (pow(2, QUANT_BITS_LOCAL_CODE - 1) - 1) / max));
                    tmp2 = static_cast<int32_t>(floor(fft_codes_padded[i].imag() * (pow(2, QUANT_BITS_LOCAL_CODE - 1) - 1) / max));
                    local_code = (tmp & SELECT_LSBITS) | ((tmp2 * SHL_CODE_BITS) & SELECT_MSBITS);  // put together the real part and the imaginary part
                    fft_data = local_code & SELECT_ALL_CODE_BITS;
                    d_all_fft_codes_[i + (nsamples_total * (PRN - 1))] = fft_data;
                }
        }

    acq_parameters_.all_fft_codes = d_all_fft_codes_.data();
}

void GalileoE5aPcpsAcquisitionFpga::stop_acquisition()
{
    // stop the acquisition and the other FPGA modules.
    acquisition_fpga_->stop_acquisition();
}


void GalileoE5aPcpsAcquisitionFpga::set_threshold(float threshold)
{
    DLOG(INFO) << "Channel " << channel_ << " Threshold = " << threshold;
    acquisition_fpga_->set_threshold(threshold);
}


void GalileoE5aPcpsAcquisitionFpga::set_doppler_max(unsigned int doppler_max)
{
    doppler_max_ = doppler_max;
    acquisition_fpga_->set_doppler_max(doppler_max_);
}


void GalileoE5aPcpsAcquisitionFpga::set_doppler_step(unsigned int doppler_step)
{
    doppler_step_ = doppler_step;
    acquisition_fpga_->set_doppler_step(doppler_step_);
}


void GalileoE5aPcpsAcquisitionFpga::set_doppler_center(int doppler_center)
{
    doppler_center_ = doppler_center;

    acquisition_fpga_->set_doppler_center(doppler_center_);
}


void GalileoE5aPcpsAcquisitionFpga::set_gnss_synchro(Gnss_Synchro* gnss_synchro)
{
    gnss_synchro_ = gnss_synchro;
    acquisition_fpga_->set_gnss_synchro(gnss_synchro_);
}


signed int GalileoE5aPcpsAcquisitionFpga::mag()
{
    return acquisition_fpga_->mag();
}


void GalileoE5aPcpsAcquisitionFpga::init()
{
    acquisition_fpga_->init();
}


void GalileoE5aPcpsAcquisitionFpga::set_local_code()
{
    acquisition_fpga_->set_local_code();
}


void GalileoE5aPcpsAcquisitionFpga::reset()
{
    acquisition_fpga_->set_active(true);
}


void GalileoE5aPcpsAcquisitionFpga::set_state(int state)
{
    acquisition_fpga_->set_state(state);
}


void GalileoE5aPcpsAcquisitionFpga::connect(gr::top_block_sptr top_block)
{
    if (top_block)
        { /* top_block is not null */
        };
    // Nothing to connect
}


void GalileoE5aPcpsAcquisitionFpga::disconnect(gr::top_block_sptr top_block)
{
    if (top_block)
        { /* top_block is not null */
        };
    // Nothing to disconnect
}


gr::basic_block_sptr GalileoE5aPcpsAcquisitionFpga::get_left_block()
{
    return nullptr;
}


gr::basic_block_sptr GalileoE5aPcpsAcquisitionFpga::get_right_block()
{
    return nullptr;
}
