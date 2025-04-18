/*!
 * \file pcps_acquisition_fine_doppler_cc.cc
 * \brief This class implements a Parallel Code Phase Search Acquisition with multi-dwells and fine Doppler estimation
 * \authors <ul>
 *          <li> Javier Arribas, 2013. jarribas(at)cttc.es
 *          </ul>
 *
 * -----------------------------------------------------------------------------
 *
 * GNSS-SDR is a Global Navigation Satellite System software-defined receiver.
 * This file is part of GNSS-SDR.
 *
 * Copyright (C) 2010-2020  (see AUTHORS file for a list of contributors)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * -----------------------------------------------------------------------------
 */

#include "pcps_acquisition_fine_doppler_cc.h"
#include "GPS_L1_CA.h"  // for GPS_L1_CA_CHIP_PERIOD_S
#include "gnss_sdr_create_directory.h"
#include "gnss_sdr_filesystem.h"
#include "gps_sdr_signal_replica.h"
#include <gnuradio/io_signature.h>
#include <matio.h>
#include <volk/volk.h>
#include <algorithm>  // std::rotate, std::fill_n
#include <array>
#include <sstream>
#include <vector>

#if USE_GLOG_AND_GFLAGS
#include <glog/logging.h>
#else
#include <absl/log/log.h>
#endif


pcps_acquisition_fine_doppler_cc_sptr pcps_make_acquisition_fine_doppler_cc(const Acq_Conf &conf_)
{
    return pcps_acquisition_fine_doppler_cc_sptr(
        new pcps_acquisition_fine_doppler_cc(conf_));
}


pcps_acquisition_fine_doppler_cc::pcps_acquisition_fine_doppler_cc(const Acq_Conf &conf_)
    : gr::block("pcps_acquisition_fine_doppler_cc",
          gr::io_signature::make(1, 1, sizeof(gr_complex)),
          gr::io_signature::make(0, 1, sizeof(Gnss_Synchro))),
      d_dump_filename(conf_.dump_filename),
      d_gnss_synchro(nullptr),
      acq_parameters(conf_),
      d_fs_in(conf_.fs_in),
      d_dump_number(0),
      d_sample_counter(0ULL),
      d_threshold(0),
      d_test_statistics(0),
      d_positive_acq(0),
      d_state(0),
      d_samples_per_ms(static_cast<int>(conf_.samples_per_ms)),
      d_max_dwells(conf_.max_dwells),
      d_config_doppler_max(conf_.doppler_max),
      d_num_doppler_points(0),
      d_well_count(0),
      d_n_samples_in_buffer(0),
      d_fft_size(d_samples_per_ms),
      d_gnuradio_forecast_samples(d_fft_size),
      d_doppler_step(0),
      d_channel(0),
      d_dump_channel(0),
      d_active(false),
      d_dump(conf_.dump)
{
    this->message_port_register_out(pmt::mp("events"));

    d_fft_codes = volk_gnsssdr::vector<gr_complex>(d_fft_size);
    d_magnitude = volk_gnsssdr::vector<float>(d_fft_size);
    d_10_ms_buffer = volk_gnsssdr::vector<gr_complex>(50 * d_samples_per_ms);
    d_fft_if = gnss_fft_fwd_make_unique(d_fft_size);
    d_ifft = gnss_fft_rev_make_unique(d_fft_size);

    // this implementation can only produce dumps in channel 0
    // todo: migrate config parameters to the unified acquisition config class
    if (d_dump)
        {
            std::string dump_path;
            // Get path
            if (d_dump_filename.find_last_of('/') != std::string::npos)
                {
                    std::string dump_filename_ = d_dump_filename.substr(d_dump_filename.find_last_of('/') + 1);
                    dump_path = d_dump_filename.substr(0, d_dump_filename.find_last_of('/'));
                    d_dump_filename = std::move(dump_filename_);
                }
            else
                {
                    dump_path = std::string(".");
                }
            if (d_dump_filename.empty())
                {
                    d_dump_filename = "acquisition";
                }
            // remove extension if any
            if (d_dump_filename.substr(1).find_last_of('.') != std::string::npos)
                {
                    d_dump_filename = d_dump_filename.substr(0, d_dump_filename.find_last_of('.'));
                }
            d_dump_filename = dump_path + fs::path::preferred_separator + d_dump_filename;
            // create directory
            if (!gnss_sdr_create_directory(dump_path))
                {
                    std::cerr << "GNSS-SDR cannot create dump file for the Acquisition block. Wrong permissions?\n";
                    d_dump = false;
                }
        }
}


// Finds next power of two
// for n. If n itself is a
// power of two then returns n
unsigned int pcps_acquisition_fine_doppler_cc::nextPowerOf2(unsigned int n)
{
    n--;
    n |= n >> 1U;
    n |= n >> 2U;
    n |= n >> 4U;
    n |= n >> 8U;
    n |= n >> 16U;
    n++;
    return n;
}


void pcps_acquisition_fine_doppler_cc::set_doppler_step(unsigned int doppler_step)
{
    d_doppler_step = doppler_step;
    // Create the search grid array

    d_num_doppler_points = floor(std::abs(2 * d_config_doppler_max) / d_doppler_step);

    d_grid_data = volk_gnsssdr::vector<volk_gnsssdr::vector<float>>(d_num_doppler_points, volk_gnsssdr::vector<float>(d_fft_size));

    if (d_dump)
        {
            grid_ = arma::fmat(d_fft_size, d_num_doppler_points, arma::fill::zeros);
        }

    update_carrier_wipeoff();
}


void pcps_acquisition_fine_doppler_cc::set_local_code(std::complex<float> *code)
{
    std::copy(code, code + d_fft_size, d_fft_if->get_inbuf());
    d_fft_if->execute();  // We need the FFT of local code
    // Conjugate the local code
    volk_32fc_conjugate_32fc(d_fft_codes.data(), d_fft_if->get_outbuf(), d_fft_size);
}


void pcps_acquisition_fine_doppler_cc::init()
{
    d_gnss_synchro->Flag_valid_acquisition = false;
    d_gnss_synchro->Flag_valid_symbol_output = false;
    d_gnss_synchro->Flag_valid_pseudorange = false;
    d_gnss_synchro->Flag_valid_word = false;
    d_gnss_synchro->Acq_doppler_step = 0U;
    d_gnss_synchro->Acq_delay_samples = 0.0;
    d_gnss_synchro->Acq_doppler_hz = 0.0;
    d_gnss_synchro->Acq_samplestamp_samples = 0ULL;
    d_state = 0;
}


void pcps_acquisition_fine_doppler_cc::forecast(int noutput_items,
    gr_vector_int &ninput_items_required)
{
    if (noutput_items != 0)
        {
            ninput_items_required[0] = d_gnuradio_forecast_samples;  // set the required available samples in each call
        }
}


void pcps_acquisition_fine_doppler_cc::reset_grid()
{
    d_well_count = 0;
    for (int i = 0; i < d_num_doppler_points; i++)
        {
            // todo: use memset here
            for (int j = 0; j < d_fft_size; j++)
                {
                    d_grid_data[i][j] = 0.0;
                }
        }
}


void pcps_acquisition_fine_doppler_cc::update_carrier_wipeoff()
{
    // create the carrier Doppler wipeoff signals
    int doppler_hz;
    float phase_step_rad;
    d_grid_doppler_wipeoffs = volk_gnsssdr::vector<volk_gnsssdr::vector<std::complex<float>>>(d_num_doppler_points, volk_gnsssdr::vector<std::complex<float>>(d_fft_size));
    for (int doppler_index = 0; doppler_index < d_num_doppler_points; doppler_index++)
        {
            doppler_hz = static_cast<int>(d_doppler_step) * doppler_index - d_config_doppler_max;
            // doppler search steps
            // compute the carrier doppler wipe-off signal and store it
            phase_step_rad = static_cast<float>(TWO_PI) * static_cast<float>(doppler_hz) / static_cast<float>(d_fs_in);
            float _phase[1];
            _phase[0] = 0;
            volk_gnsssdr_s32f_sincos_32fc(d_grid_doppler_wipeoffs[doppler_index].data(), -phase_step_rad, _phase, d_fft_size);
        }
}


float pcps_acquisition_fine_doppler_cc::compute_CAF()
{
    float firstPeak = 0.0;
    int index_doppler = 0;
    uint32_t tmp_intex_t = 0;
    uint32_t index_time = 0;

    // Look for correlation peaks in the results ==============================
    // Find the highest peak and compare it to the second highest peak
    // The second peak is chosen not closer than 1 chip to the highest peak
    // --- Find the correlation peak and the carrier frequency --------------
    for (int i = 0; i < d_num_doppler_points; i++)
        {
            volk_gnsssdr_32f_index_max_32u(&tmp_intex_t, d_grid_data[i].data(), d_fft_size);
            if (d_grid_data[i][tmp_intex_t] > firstPeak)
                {
                    firstPeak = d_grid_data[i][tmp_intex_t];
                    index_doppler = i;
                    index_time = tmp_intex_t;
                }

            // Record results to file if required
            if (d_dump and d_channel == d_dump_channel)
                {
                    std::copy(d_grid_data[i].data(), d_grid_data[i].data() + d_fft_size, grid_.colptr(i));
                }
        }

    // -- - Find 1 chip wide code phase exclude range around the peak
    uint32_t samplesPerChip = ceil(GPS_L1_CA_CHIP_PERIOD_S * static_cast<float>(this->d_fs_in));
    int32_t excludeRangeIndex1 = index_time - samplesPerChip;
    int32_t excludeRangeIndex2 = index_time + samplesPerChip;

    // -- - Correct code phase exclude range if the range includes array boundaries
    if (excludeRangeIndex1 < 0)
        {
            excludeRangeIndex1 = d_fft_size + excludeRangeIndex1;
        }
    else if (excludeRangeIndex2 >= static_cast<int>(d_fft_size))
        {
            excludeRangeIndex2 = excludeRangeIndex2 - d_fft_size;
        }

    int32_t idx = excludeRangeIndex1;
    do
        {
            d_grid_data[index_doppler][idx] = 0.0;
            idx++;
            if (idx == static_cast<int>(d_fft_size))
                {
                    idx = 0;
                }
        }
    while (idx != excludeRangeIndex2);

    // --- Find the second highest correlation peak in the same freq. bin ---
    volk_gnsssdr_32f_index_max_32u(&tmp_intex_t, d_grid_data[index_doppler].data(), d_fft_size);
    float secondPeak = d_grid_data[index_doppler][tmp_intex_t];

    // 5- Compute the test statistics and compare to the threshold
    d_test_statistics = firstPeak / secondPeak;

    // 4- record the maximum peak and the associated synchronization parameters
    d_gnss_synchro->Acq_delay_samples = static_cast<double>(index_time);
    d_gnss_synchro->Acq_doppler_hz = static_cast<double>(index_doppler * d_doppler_step - d_config_doppler_max);
    d_gnss_synchro->Acq_samplestamp_samples = d_sample_counter;
    d_gnss_synchro->Acq_doppler_step = d_doppler_step;

    return d_test_statistics;
}


float pcps_acquisition_fine_doppler_cc::estimate_input_power(gr_vector_const_void_star &input_items)
{
    const auto *in = reinterpret_cast<const gr_complex *>(input_items[0]);  // Get the input samples pointer
    // Compute the input signal power estimation
    float power = 0;
    volk_32fc_magnitude_squared_32f(d_magnitude.data(), in, d_fft_size);
    volk_32f_accumulator_s32f(&power, d_magnitude.data(), d_fft_size);
    power /= static_cast<float>(d_fft_size);
    return power;
}


int pcps_acquisition_fine_doppler_cc::compute_and_accumulate_grid(gr_vector_const_void_star &input_items)
{
    // initialize acquisition algorithm
    const auto *in = reinterpret_cast<const gr_complex *>(input_items[0]);  // Get the input samples pointer

    DLOG(INFO) << "Channel: " << d_channel
               << " , doing acquisition of satellite: " << d_gnss_synchro->System << " " << d_gnss_synchro->PRN
               << " ,sample stamp: " << d_sample_counter << ", threshold: "
               << d_threshold << ", doppler_max: " << d_config_doppler_max
               << ", doppler_step: " << d_doppler_step;

    // 2- Doppler frequency search loop
    volk_gnsssdr::vector<float> p_tmp_vector(d_fft_size);

    for (int doppler_index = 0; doppler_index < d_num_doppler_points; doppler_index++)
        {
            // doppler search steps
            // Perform the carrier wipe-off
            volk_32fc_x2_multiply_32fc(d_fft_if->get_inbuf(), in, d_grid_doppler_wipeoffs[doppler_index].data(), d_fft_size);

            // 3- Perform the FFT-based convolution  (parallel time search)
            // Compute the FFT of the carrier wiped--off incoming signal
            d_fft_if->execute();

            // Multiply carrier wiped--off, Fourier transformed incoming signal
            // with the local FFT'd code reference using SIMD operations with VOLK library
            volk_32fc_x2_multiply_32fc(d_ifft->get_inbuf(), d_fft_if->get_outbuf(), d_fft_codes.data(), d_fft_size);

            // compute the inverse FFT
            d_ifft->execute();

            // save the grid matrix delay file
            volk_32fc_magnitude_squared_32f(p_tmp_vector.data(), d_ifft->get_outbuf(), d_fft_size);
            // accumulate grid values
            volk_32f_x2_add_32f(d_grid_data[doppler_index].data(), d_grid_data[doppler_index].data(), p_tmp_vector.data(), d_fft_size);
        }

    return d_fft_size;
    // debug
    //            std::cout << "iff=[";
    //            for (int n = 0; n < d_fft_size; n++)
    //                {
    //                    std::cout << std::real(d_ifft->get_outbuf()[n]) << "+" << std::imag(d_ifft->get_outbuf()[n]) << "i,";
    //                }
    //            std::cout << "]\n";
    //            getchar();
}


int pcps_acquisition_fine_doppler_cc::estimate_Doppler()
{
    // Direct FFT
    int zero_padding_factor = 8;
    int prn_replicas = 10;
    int signal_samples = prn_replicas * d_fft_size;
    // int fft_size_extended = nextPowerOf2(signal_samples * zero_padding_factor);
    int fft_size_extended = signal_samples * zero_padding_factor;

    auto fft_operator = gnss_fft_fwd_make_unique(fft_size_extended);

    // zero padding the entire vector
    std::fill_n(fft_operator->get_inbuf(), fft_size_extended, gr_complex(0.0, 0.0));

    // 1. generate local code aligned with the acquisition code phase estimation
    volk_gnsssdr::vector<gr_complex> code_replica(signal_samples);

    gps_l1_ca_code_gen_complex_sampled(code_replica, d_gnss_synchro->PRN, d_fs_in, 0);

    int shift_index = static_cast<int>(d_gnss_synchro->Acq_delay_samples);

    // Rotate to align the local code replica using acquisition time delay estimation
    if (shift_index != 0)
        {
            std::rotate(code_replica.data(), code_replica.data() + (d_fft_size - shift_index), code_replica.data() + d_fft_size - 1);
        }

    for (int n = 0; n < prn_replicas - 1; n++)
        {
            std::copy_n(code_replica.data(), d_fft_size, &code_replica[(n + 1) * d_fft_size]);
        }
    // 2. Perform code wipe-off
    volk_32fc_x2_multiply_32fc(fft_operator->get_inbuf(), d_10_ms_buffer.data(), code_replica.data(), signal_samples);

    // 3. Perform the FFT (zero padded!)
    fft_operator->execute();

    // 4. Compute the magnitude and find the maximum
    volk_gnsssdr::vector<float> p_tmp_vector(fft_size_extended);
    volk_32fc_magnitude_squared_32f(p_tmp_vector.data(), fft_operator->get_outbuf(), fft_size_extended);

    uint32_t tmp_index_freq = 0;
    volk_gnsssdr_32f_index_max_32u(&tmp_index_freq, p_tmp_vector.data(), fft_size_extended);

    // case even
    int counter = 0;
    volk_gnsssdr::vector<float> fftFreqBins(fft_size_extended);

    for (int k = 0; k < (fft_size_extended / 2); k++)
        {
            fftFreqBins[counter] = ((static_cast<float>(d_fs_in) / 2.0) * static_cast<float>(k)) / (static_cast<float>(fft_size_extended) / 2.0);
            counter++;
        }

    for (int k = fft_size_extended / 2; k > 0; k--)
        {
            fftFreqBins[counter] = ((-static_cast<float>(d_fs_in) / 2.0) * static_cast<float>(k)) / (static_cast<float>(fft_size_extended) / 2.0);
            counter++;
        }

    // 5. Update the Doppler estimation in Hz
    if (std::abs(fftFreqBins[tmp_index_freq] - d_gnss_synchro->Acq_doppler_hz) < 1000)
        {
            d_gnss_synchro->Acq_doppler_hz = static_cast<double>(fftFreqBins[tmp_index_freq]);
            // std::cout << "FFT maximum present at " << fftFreqBins[tmp_index_freq] << " [Hz]\n";
        }
    else
        {
            DLOG(INFO) << "Abs(Grid Doppler - FFT Doppler)=" << std::abs(fftFreqBins[tmp_index_freq] - d_gnss_synchro->Acq_doppler_hz);
            DLOG(INFO) << "Error estimating fine frequency Doppler";
        }

    return d_fft_size;
}


// Called by gnuradio to enable drivers, etc for i/o devices.
bool pcps_acquisition_fine_doppler_cc::start()
{
    d_sample_counter = 0ULL;
    return true;
}


void pcps_acquisition_fine_doppler_cc::set_state(int state)
{
    // gr::thread::scoped_lock lock(d_setlock);  // require mutex with work function called by the scheduler
    d_state = state;

    if (d_state == 1)
        {
            d_gnss_synchro->Acq_delay_samples = 0.0;
            d_gnss_synchro->Acq_doppler_hz = 0.0;
            d_gnss_synchro->Acq_samplestamp_samples = 0ULL;
            d_gnss_synchro->Acq_doppler_step = 0U;
            d_well_count = 0;
            d_test_statistics = 0.0;
            d_active = true;
            reset_grid();
        }
    else if (d_state == 0)
        {
        }
    else
        {
            LOG(ERROR) << "State can only be set to 0 or 1";
        }
}


int pcps_acquisition_fine_doppler_cc::general_work(int noutput_items,
    gr_vector_int &ninput_items __attribute__((unused)), gr_vector_const_void_star &input_items,
    gr_vector_void_star &output_items)
{
    /*!
     * TODO:     High sensitivity acquisition algorithm:
     *             State Machine:
     *             S0. StandBy. If d_active==1 -> S1
     *             S1. ComputeGrid. Perform the FFT acquisition doppler and delay grid.
     *                 Accumulate the search grid matrix (#doppler_bins x #fft_size)
     *                 Compare maximum to threshold and decide positive or negative
     *                 If T>=gamma -> S4 else
     *                 If d_well_count<max_dwells -> S2
     *                 else -> S5.
     *             S4. Positive_Acq: Send message and stop acq -> S0
     *             S5. Negative_Acq: Send message and stop acq -> S0
     */

    int return_value = 0;  // Number of Gnss_Syncro objects produced
    int samples_remaining;
    const auto *in_aux = reinterpret_cast<const gr_complex *>(input_items[0]);
    switch (d_state)
        {
        case 0:  // S0. StandBy
            if (d_active == true)
                {
                    reset_grid();
                    d_n_samples_in_buffer = 0;
                    d_state = 1;
                }
            if (!acq_parameters.blocking_on_standby)
                {
                    d_sample_counter += static_cast<uint64_t>(d_fft_size);  // sample counter
                    consume_each(d_fft_size);
                }
            break;
        case 1:  // S1. ComputeGrid
            compute_and_accumulate_grid(input_items);
            std::copy(in_aux, in_aux + d_fft_size, &d_10_ms_buffer[d_n_samples_in_buffer]);
            d_n_samples_in_buffer += d_fft_size;
            d_well_count++;
            if (d_well_count >= d_max_dwells)
                {
                    d_state = 2;
                }
            d_sample_counter += static_cast<uint64_t>(d_fft_size);  // sample counter
            consume_each(d_fft_size);
            break;
        case 2:  // Compute test statistics and decide
            d_test_statistics = compute_CAF();
            if (d_test_statistics > d_threshold)
                {
                    d_state = 3;  // perform fine doppler estimation
                }
            else
                {
                    d_state = 5;  // negative acquisition
                    d_n_samples_in_buffer = 0;
                }

            break;
        case 3:  // Fine doppler estimation
            samples_remaining = 10 * d_samples_per_ms - d_n_samples_in_buffer;

            if (samples_remaining > noutput_items)
                {
                    std::copy(in_aux, in_aux + noutput_items, &d_10_ms_buffer[d_n_samples_in_buffer]);
                    d_n_samples_in_buffer += noutput_items;
                    d_sample_counter += static_cast<uint64_t>(noutput_items);  // sample counter
                    consume_each(noutput_items);
                }
            else
                {
                    if (samples_remaining > 0)
                        {
                            std::copy(in_aux, in_aux + samples_remaining, &d_10_ms_buffer[d_n_samples_in_buffer]);
                            d_sample_counter += static_cast<uint64_t>(samples_remaining);  // sample counter
                            consume_each(samples_remaining);
                        }
                    estimate_Doppler();  // disabled in repo
                    d_n_samples_in_buffer = 0;
                    d_state = 4;
                }
            break;
        case 4:  // Positive_Acq
            DLOG(INFO) << "positive acquisition";
            DLOG(INFO) << "satellite " << d_gnss_synchro->System << " " << d_gnss_synchro->PRN;
            DLOG(INFO) << "sample_stamp " << d_sample_counter;
            DLOG(INFO) << "test statistics value " << d_test_statistics;
            DLOG(INFO) << "test statistics threshold " << d_threshold;
            DLOG(INFO) << "code phase " << d_gnss_synchro->Acq_delay_samples;
            DLOG(INFO) << "doppler " << d_gnss_synchro->Acq_doppler_hz;
            d_positive_acq = 1;
            d_active = false;
            // Record results to file if required
            if (d_dump and d_channel == d_dump_channel)
                {
                    dump_results(d_fft_size);
                }
            // Send message to channel port //0=STOP_CHANNEL 1=ACQ_SUCCEES 2=ACQ_FAIL
            this->message_port_pub(pmt::mp("events"), pmt::from_long(1));
            d_state = 0;
            if (!acq_parameters.blocking_on_standby)
                {
                    d_sample_counter += static_cast<uint64_t>(noutput_items);  // sample counter
                    consume_each(noutput_items);
                }
            // Copy and push current Gnss_Synchro to monitor queue
            if (acq_parameters.enable_monitor_output)
                {
                    auto **out = reinterpret_cast<Gnss_Synchro **>(&output_items[0]);
                    Gnss_Synchro current_synchro_data = Gnss_Synchro();
                    current_synchro_data = *d_gnss_synchro;
                    *out[0] = std::move(current_synchro_data);
                    return_value = 1;  // Number of Gnss_Synchro objects produced
                }
            break;
        case 5:  // Negative_Acq
            DLOG(INFO) << "negative acquisition";
            DLOG(INFO) << "satellite " << d_gnss_synchro->System << " " << d_gnss_synchro->PRN;
            DLOG(INFO) << "sample_stamp " << d_sample_counter;
            DLOG(INFO) << "test statistics value " << d_test_statistics;
            DLOG(INFO) << "test statistics threshold " << d_threshold;
            DLOG(INFO) << "code phase " << d_gnss_synchro->Acq_delay_samples;
            DLOG(INFO) << "doppler " << d_gnss_synchro->Acq_doppler_hz;
            d_positive_acq = 0;
            d_active = false;
            // Record results to file if required
            if (d_dump and d_channel == d_dump_channel)
                {
                    dump_results(d_fft_size);
                }
            // Send message to channel port //0=STOP_CHANNEL 1=ACQ_SUCCEES 2=ACQ_FAIL
            this->message_port_pub(pmt::mp("events"), pmt::from_long(2));
            d_state = 0;
            if (!acq_parameters.blocking_on_standby)
                {
                    d_sample_counter += static_cast<uint64_t>(noutput_items);  // sample counter
                    consume_each(noutput_items);
                }
            break;
        default:
            d_state = 0;
            if (!acq_parameters.blocking_on_standby)
                {
                    d_sample_counter += static_cast<uint64_t>(noutput_items);  // sample counter
                    consume_each(noutput_items);
                }
            break;
        }
    return return_value;
}

void pcps_acquisition_fine_doppler_cc::dump_results(int effective_fft_size)
{
    d_dump_number++;
    std::string filename = d_dump_filename;
    filename.append("_");
    filename.append(1, d_gnss_synchro->System);
    filename.append("_");
    filename.append(1, d_gnss_synchro->Signal[0]);
    filename.append(1, d_gnss_synchro->Signal[1]);
    filename.append("_ch_");
    filename.append(std::to_string(d_channel));
    filename.append("_");
    filename.append(std::to_string(d_dump_number));
    filename.append("_sat_");
    filename.append(std::to_string(d_gnss_synchro->PRN));
    filename.append(".mat");

    mat_t *matfp = Mat_CreateVer(filename.c_str(), nullptr, MAT_FT_MAT73);
    if (matfp == nullptr)
        {
            std::cout << "Unable to create or open Acquisition dump file\n";
            d_dump = false;
        }
    else
        {
            std::array<size_t, 2> dims{static_cast<size_t>(effective_fft_size), static_cast<size_t>(d_num_doppler_points)};
            matvar_t *matvar = Mat_VarCreate("acq_grid", MAT_C_SINGLE, MAT_T_SINGLE, 2, dims.data(), grid_.memptr(), 0);
            Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);  // or MAT_COMPRESSION_NONE
            Mat_VarFree(matvar);

            dims[0] = static_cast<size_t>(1);
            dims[1] = static_cast<size_t>(1);
            matvar = Mat_VarCreate("doppler_max", MAT_C_INT32, MAT_T_INT32, 1, dims.data(), &d_config_doppler_max, 0);
            Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);  // or MAT_COMPRESSION_NONE
            Mat_VarFree(matvar);

            matvar = Mat_VarCreate("doppler_step", MAT_C_INT32, MAT_T_INT32, 1, dims.data(), &d_doppler_step, 0);
            Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);  // or MAT_COMPRESSION_NONE
            Mat_VarFree(matvar);

            matvar = Mat_VarCreate("d_positive_acq", MAT_C_INT32, MAT_T_INT32, 1, dims.data(), &d_positive_acq, 0);
            Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);  // or MAT_COMPRESSION_NONE
            Mat_VarFree(matvar);

            auto aux = static_cast<float>(d_gnss_synchro->Acq_doppler_hz);
            matvar = Mat_VarCreate("acq_doppler_hz", MAT_C_SINGLE, MAT_T_SINGLE, 1, dims.data(), &aux, 0);
            Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);  // or MAT_COMPRESSION_NONE
            Mat_VarFree(matvar);

            aux = static_cast<float>(d_gnss_synchro->Acq_delay_samples);
            matvar = Mat_VarCreate("acq_delay_samples", MAT_C_SINGLE, MAT_T_SINGLE, 1, dims.data(), &aux, 0);
            Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);  // or MAT_COMPRESSION_NONE
            Mat_VarFree(matvar);

            matvar = Mat_VarCreate("test_statistic", MAT_C_SINGLE, MAT_T_SINGLE, 1, dims.data(), &d_test_statistics, 0);
            Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);  // or MAT_COMPRESSION_NONE
            Mat_VarFree(matvar);

            matvar = Mat_VarCreate("threshold", MAT_C_SINGLE, MAT_T_SINGLE, 1, dims.data(), &d_threshold, 0);
            Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);  // or MAT_COMPRESSION_NONE
            Mat_VarFree(matvar);
            aux = 0.0;
            matvar = Mat_VarCreate("input_power", MAT_C_SINGLE, MAT_T_SINGLE, 1, dims.data(), &aux, 0);
            Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);  // or MAT_COMPRESSION_NONE
            Mat_VarFree(matvar);

            matvar = Mat_VarCreate("sample_counter", MAT_C_UINT64, MAT_T_UINT64, 1, dims.data(), &d_sample_counter, 0);
            Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);  // or MAT_COMPRESSION_NONE
            Mat_VarFree(matvar);

            matvar = Mat_VarCreate("PRN", MAT_C_UINT32, MAT_T_UINT32, 1, dims.data(), &d_gnss_synchro->PRN, 0);
            Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);  // or MAT_COMPRESSION_NONE
            Mat_VarFree(matvar);

            Mat_Close(matfp);
        }
}
