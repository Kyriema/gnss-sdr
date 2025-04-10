/*!
 * \file galileo_e1_pcps_ambiguous_acquisition_test.cc
 * \brief  This class implements an acquisition test for
 * GalileoE1PcpsAmbiguousAcquisition class based on some input parameters.
 * \author Luis Esteve, 2012. luis(at)epsilon-formacion.com
 *
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


#include "Galileo_E1.h"
#include "acquisition_dump_reader.h"
#include "concurrent_queue.h"
#include "galileo_e1_pcps_ambiguous_acquisition.h"
#include "gnss_block_factory.h"
#include "gnss_block_interface.h"
#include "gnss_sdr_filesystem.h"
#include "gnss_sdr_valve.h"
#include "gnss_signal.h"
#include "gnss_synchro.h"
#include "gnuplot_i.h"
#include "in_memory_configuration.h"
#include "test_flags.h"
#include <boost/make_shared.hpp>
#include <gnuradio/analog/sig_source_waveform.h>
#include <gnuradio/blocks/file_source.h>
#include <gnuradio/blocks/null_sink.h>
#include <gnuradio/top_block.h>
#include <gtest/gtest.h>
#include <pmt/pmt.h>
#include <chrono>
#include <utility>

#if HAS_GENERIC_LAMBDA
#else
#include <boost/bind/bind.hpp>
#endif

#ifdef GR_GREATER_38
#include <gnuradio/analog/sig_source.h>
#else
#include <gnuradio/analog/sig_source_c.h>
#endif

#if PMT_USES_BOOST_ANY
namespace wht = boost;
#else
namespace wht = std;
#endif


// ######## GNURADIO BLOCK MESSAGE RECEIVER #########
class GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx;

using GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx_sptr = gnss_shared_ptr<GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx>;

GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx_sptr GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx_make();

class GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx : public gr::block
{
private:
    friend GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx_sptr GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx_make();
    void msg_handler_channel_events(const pmt::pmt_t msg);
    GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx();

public:
    int rx_message;
    ~GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx();  //!< Default destructor
};


GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx_sptr GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx_make()
{
    return GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx_sptr(new GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx());
}


void GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx::msg_handler_channel_events(const pmt::pmt_t msg)
{
    try
        {
            int64_t message = pmt::to_long(std::move(msg));
            rx_message = message;
        }
    catch (const wht::bad_any_cast& e)
        {
            LOG(WARNING) << "msg_handler_channel_events Bad any_cast: " << e.what();
            rx_message = 0;
        }
}


GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx::GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx() : gr::block("GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx", gr::io_signature::make(0, 0, 0), gr::io_signature::make(0, 0, 0))
{
    this->message_port_register_in(pmt::mp("events"));
    this->set_msg_handler(pmt::mp("events"),
#if HAS_GENERIC_LAMBDA
        [this](auto&& PH1) { msg_handler_channel_events(PH1); });
#else
#if USE_BOOST_BIND_PLACEHOLDERS
        boost::bind(&GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx::msg_handler_channel_events, this, boost::placeholders::_1));
#else
        boost::bind(&GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx::msg_handler_channel_events, this, _1));
#endif
#endif
    rx_message = 0;
}


GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx::~GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx() = default;


// ###########################################################

class GalileoE1PcpsAmbiguousAcquisitionTest : public ::testing::Test
{
protected:
    GalileoE1PcpsAmbiguousAcquisitionTest()
    {
        factory = std::make_shared<GNSSBlockFactory>();
        config = std::make_shared<InMemoryConfiguration>();
        item_size = sizeof(gr_complex);
        gnss_synchro = Gnss_Synchro();
        doppler_max = 10000;
        doppler_step = 250;
    }

    ~GalileoE1PcpsAmbiguousAcquisitionTest() = default;

    void init();
    void plot_grid();

    gr::top_block_sptr top_block;
    std::shared_ptr<GNSSBlockFactory> factory;
    std::shared_ptr<InMemoryConfiguration> config;
    Gnss_Synchro gnss_synchro;
    size_t item_size;
    unsigned int doppler_max;
    unsigned int doppler_step;
};


void GalileoE1PcpsAmbiguousAcquisitionTest::init()
{
    gnss_synchro.Channel_ID = 0;
    gnss_synchro.System = 'E';
    std::string signal = "1B";
    signal.copy(gnss_synchro.Signal, 2, 0);
    gnss_synchro.PRN = 1;

    config->set_property("Acquisition_1B.implementation", "Galileo_E1_PCPS_Ambiguous_Acquisition");
    config->set_property("GNSS-SDR.internal_fs_sps", "4000000");
    config->set_property("Acquisition_1B.item_type", "gr_complex");
    config->set_property("Acquisition_1B.coherent_integration_time_ms", "4");
#if USE_GLOG_AND_GFLAGS
    if (FLAGS_plot_acq_grid == true)
#else
    if (absl::GetFlag(FLAGS_plot_acq_grid) == true)
#endif
        {
            config->set_property("Acquisition_1B.dump", "true");
        }
    else
        {
            config->set_property("Acquisition_1B.dump", "false");
        }
    config->set_property("Acquisition_1B.dump_filename", "./tmp-acq-gal1/acquisition");
    // config->set_property("Acquisition_1B.threshold", "2.5");
    config->set_property("Acquisition_1B.pfa", "0.001");
    config->set_property("Acquisition_1B.doppler_max", std::to_string(doppler_max));
    config->set_property("Acquisition_1B.doppler_step", std::to_string(doppler_step));
    config->set_property("Acquisition_1B.repeat_satellite", "false");
    config->set_property("Acquisition_1B.cboc", "true");
}


void GalileoE1PcpsAmbiguousAcquisitionTest::plot_grid()
{
    // load the measured values
    std::string basename = "./tmp-acq-gal1/acquisition_E_1B";
    auto sat = static_cast<unsigned int>(gnss_synchro.PRN);

    auto samples_per_code = static_cast<unsigned int>(round(4000000 / (GALILEO_E1_CODE_CHIP_RATE_CPS / GALILEO_E1_B_CODE_LENGTH_CHIPS)));  // !!
    Acquisition_Dump_Reader acq_dump(basename, sat, doppler_max, doppler_step, samples_per_code);

    if (!acq_dump.read_binary_acq())
        {
            std::cout << "Error reading files\n";
        }

    std::vector<int>* doppler = &acq_dump.doppler;
    std::vector<unsigned int>* samples = &acq_dump.samples;
    std::vector<std::vector<float>>* mag = &acq_dump.mag;

#if USE_GLOG_AND_GFLAGS
    const std::string gnuplot_executable(FLAGS_gnuplot_executable);
#else
    const std::string gnuplot_executable(absl::GetFlag(FLAGS_gnuplot_executable));
#endif
    if (gnuplot_executable.empty())
        {
            std::cout << "WARNING: Although the flag plot_acq_grid has been set to TRUE,\n";
            std::cout << "gnuplot has not been found in your system.\n";
            std::cout << "Test results will not be plotted.\n";
        }
    else
        {
            std::cout << "Plotting the acquisition grid. This can take a while...\n";
            try
                {
                    fs::path p(gnuplot_executable);
                    fs::path dir = p.parent_path();
                    const std::string& gnuplot_path = dir.native();
                    Gnuplot::set_GNUPlotPath(gnuplot_path);

                    Gnuplot g1("lines");
#if USE_GLOG_AND_GFLAGS
                    if (FLAGS_show_plots)
#else
                    if (absl::GetFlag(FLAGS_show_plots))
#endif
                        {
                            g1.showonscreen();  // window output
                        }
                    else
                        {
                            g1.disablescreen();
                        }
                    g1.set_title("Galileo E1b/c signal acquisition for satellite PRN #" + std::to_string(gnss_synchro.PRN));
                    g1.set_xlabel("Doppler [Hz]");
                    g1.set_ylabel("Sample");
                    // g1.cmd("set view 60, 105, 1, 1");
                    g1.plot_grid3d(*doppler, *samples, *mag);

                    g1.savetops("Galileo_E1_acq_grid");
                    g1.savetopdf("Galileo_E1_acq_grid");
                }
            catch (const GnuplotException& ge)
                {
                    std::cout << ge.what() << '\n';
                }
        }
    std::string data_str = "./tmp-acq-gal1";
    if (fs::exists(data_str))
        {
            fs::remove_all(data_str);
        }
}


TEST_F(GalileoE1PcpsAmbiguousAcquisitionTest, Instantiate)
{
    init();
    std::shared_ptr<GNSSBlockInterface> acq_ = factory->GetBlock(config.get(), "Acquisition_1B", 1, 0);
    std::shared_ptr<AcquisitionInterface> acquisition = std::dynamic_pointer_cast<AcquisitionInterface>(acq_);
}


TEST_F(GalileoE1PcpsAmbiguousAcquisitionTest, ConnectAndRun)
{
    int fs_in = 4000000;
    int nsamples = 4 * fs_in;
    std::chrono::time_point<std::chrono::system_clock> start, end;
    std::chrono::duration<double> elapsed_seconds(0);
    top_block = gr::make_top_block("Acquisition test");
    std::shared_ptr<Concurrent_Queue<pmt::pmt_t>> queue = std::make_shared<Concurrent_Queue<pmt::pmt_t>>();
    init();
    std::shared_ptr<GNSSBlockInterface> acq_ = factory->GetBlock(config.get(), "Acquisition_1B", 1, 0);
    std::shared_ptr<AcquisitionInterface> acquisition = std::dynamic_pointer_cast<AcquisitionInterface>(acq_);
    auto msg_rx = GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx_make();

    ASSERT_NO_THROW({
        acquisition->connect(top_block);
        auto source = gr::analog::sig_source_c::make(fs_in, gr::analog::GR_SIN_WAVE, 1000, 1, gr_complex(0));
        auto valve = gnss_sdr_make_valve(sizeof(gr_complex), nsamples, queue.get());
        top_block->connect(source, 0, valve, 0);
        top_block->connect(valve, 0, acquisition->get_left_block(), 0);
        top_block->msg_connect(acquisition->get_right_block(), pmt::mp("events"), msg_rx, pmt::mp("events"));
    }) << "Failure connecting the blocks of acquisition test.";

    EXPECT_NO_THROW({
        start = std::chrono::system_clock::now();
        top_block->run();  // Start threads and wait
        end = std::chrono::system_clock::now();
        elapsed_seconds = end - start;
    }) << "Failure running the top_block.";
    std::cout << "Processed " << nsamples << " samples in " << elapsed_seconds.count() * 1e6 << " microseconds\n";
}


TEST_F(GalileoE1PcpsAmbiguousAcquisitionTest, ValidationOfResults)
{
    std::chrono::time_point<std::chrono::system_clock> start, end;
    std::chrono::duration<double> elapsed_seconds(0);

#if USE_GLOG_AND_GFLAGS
    if (FLAGS_plot_acq_grid == true)
#else
    if (absl::GetFlag(FLAGS_plot_acq_grid) == true)
#endif
        {
            std::string data_str = "./tmp-acq-gal1";
            if (fs::exists(data_str))
                {
                    fs::remove_all(data_str);
                }
            fs::create_directory(data_str);
        }

    double expected_delay_samples = 2920;  // 18250;
    double expected_doppler_hz = -632;
    init();
    top_block = gr::make_top_block("Acquisition test");
    std::shared_ptr<GNSSBlockInterface> acq_ = factory->GetBlock(config.get(), "Acquisition_1B", 1, 0);
    std::shared_ptr<GalileoE1PcpsAmbiguousAcquisition> acquisition = std::dynamic_pointer_cast<GalileoE1PcpsAmbiguousAcquisition>(acq_);
    auto msg_rx = GalileoE1PcpsAmbiguousAcquisitionTest_msg_rx_make();

    ASSERT_NO_THROW({
        acquisition->set_channel(gnss_synchro.Channel_ID);
    }) << "Failure setting channel.";

    ASSERT_NO_THROW({
        acquisition->set_gnss_synchro(&gnss_synchro);
    }) << "Failure setting gnss_synchro.";

    ASSERT_NO_THROW({
        acquisition->set_threshold(config->property("Acquisition_1B.threshold", 1e-9));
    }) << "Failure setting threshold.";

    ASSERT_NO_THROW({
        acquisition->set_doppler_max(config->property("Acquisition_1B.doppler_max", doppler_max));
    }) << "Failure setting doppler_max.";

    ASSERT_NO_THROW({
        acquisition->set_doppler_step(config->property("Acquisition_1B.doppler_step", doppler_step));
    }) << "Failure setting doppler_step.";

    ASSERT_NO_THROW({
        acquisition->connect(top_block);
    }) << "Failure connecting acquisition to the top_block.";

    ASSERT_NO_THROW({
        std::string path = std::string(TEST_PATH);
        std::string file = path + "signal_samples/Galileo_E1_ID_1_Fs_4Msps_8ms.dat";
        const char* file_name = file.c_str();
        gr::blocks::file_source::sptr file_source = gr::blocks::file_source::make(sizeof(gr_complex), file_name, false);
        top_block->connect(file_source, 0, acquisition->get_left_block(), 0);
        top_block->msg_connect(acquisition->get_right_block(), pmt::mp("events"), msg_rx, pmt::mp("events"));
    }) << "Failure connecting the blocks of acquisition test.";

    acquisition->set_local_code();
    acquisition->init();
    acquisition->reset();
    acquisition->set_state(1);

    EXPECT_NO_THROW({
        start = std::chrono::system_clock::now();
        top_block->run();  // Start threads and wait
        end = std::chrono::system_clock::now();
        elapsed_seconds = end - start;
    }) << "Failure running the top_block.";

    uint64_t nsamples = gnss_synchro.Acq_samplestamp_samples;
    std::cout << "Acquired " << nsamples << " samples in " << elapsed_seconds.count() * 1e6 << " microseconds\n";
    ASSERT_EQ(1, msg_rx->rx_message) << "Acquisition failure. Expected message: 1=ACQ SUCCESS.";

    std::cout << "Delay: " << gnss_synchro.Acq_delay_samples << '\n';
    std::cout << "Doppler: " << gnss_synchro.Acq_doppler_hz << '\n';

    double delay_error_samples = std::abs(expected_delay_samples - gnss_synchro.Acq_delay_samples);
    auto delay_error_chips = static_cast<float>(delay_error_samples * 1023 / 4000000);
    double doppler_error_hz = std::abs(expected_doppler_hz - gnss_synchro.Acq_doppler_hz);

    EXPECT_LE(doppler_error_hz, 166) << "Doppler error exceeds the expected value: 166 Hz = 2/(3*integration period)";
    EXPECT_LT(delay_error_chips, 0.175) << "Delay error exceeds the expected value: 0.175 chips";

#if USE_GLOG_AND_GFLAGS
    if (FLAGS_plot_acq_grid == true)
#else
    if (absl::GetFlag(FLAGS_plot_acq_grid) == true)
#endif
        {
            plot_grid();
        }
}
