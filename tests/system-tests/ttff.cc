/*!
 * \file ttff.cc
 * \brief  This class implements a test for measuring
 * the Time-To-First-Fix
 * \author Carles Fernandez-Prades, 2016. cfernandez(at)cttc.es
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

#include "concurrent_map.h"
#include "concurrent_queue.h"
#include "control_thread.h"
#include "file_configuration.h"
#include "gnss_flowgraph.h"
#include "gnss_sdr_make_unique.h"
#include "gps_acq_assist.h"
#include "in_memory_configuration.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception/exception.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <gtest/gtest.h>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <string>
#include <thread>

#if USE_GLOG_AND_GFLAGS
#include <gflags/gflags.h>
#include <glog/logging.h>
#if GFLAGS_OLD_NAMESPACE
namespace gflags
{
using namespace google;
}
#endif
#else
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/log/flags.h>
#include <absl/log/log.h>
#endif


#if USE_GLOG_AND_GFLAGS
DEFINE_int32(fs_in, 4000000, "Sampling rate, in Samples/s");
DEFINE_int32(max_measurement_duration, 90, "Maximum time waiting for a position fix, in seconds");
DEFINE_int32(num_measurements, 2, "Number of measurements");
DEFINE_string(device_address, "192.168.40.2", "USRP device IP address");
DEFINE_string(subdevice, "A:0", "USRP subdevice");
DEFINE_string(config_file_ttff, std::string(""), "File containing the configuration parameters for the TTFF test.");
#else
ABSL_FLAG(int32_t, fs_in, 4000000, "Sampling rate, in Samples/s");
ABSL_FLAG(int32_t, max_measurement_duration, 90, "Maximum time waiting for a position fix, in seconds");
ABSL_FLAG(int32_t, num_measurements, 2, "Number of measurements");
ABSL_FLAG(std::string, device_address, "192.168.40.2", "USRP device IP address");
ABSL_FLAG(std::string, subdevice, "A:0", "USRP subdevice");
ABSL_FLAG(std::string, config_file_ttff, std::string(""), "File containing the configuration parameters for the TTFF test.");
#endif

// For GPS NAVIGATION (L1)
Concurrent_Queue<Gps_Acq_Assist> global_gps_acq_assist_queue;
Concurrent_Map<Gps_Acq_Assist> global_gps_acq_assist_map;

std::vector<double> TTFF_v;

class TtffTest : public ::testing::Test
{
public:
    void config_1();
    void config_2();
    void print_TTFF_report(const std::vector<double> &ttff_v, const std::shared_ptr<ConfigurationInterface> &config_);

    std::shared_ptr<InMemoryConfiguration> config;
    std::shared_ptr<FileConfiguration> config2;

    const double central_freq = 1575420000.0;
    const double gain_dB = 40.0;

    const int number_of_taps = 11;
    const int number_of_bands = 2;
    const float band1_begin = 0.0;
    const float band1_end = 0.48;
    const float band2_begin = 0.52;
    const float band2_end = 1.0;
    const float ampl1_begin = 1.0;
    const float ampl1_end = 1.0;
    const float ampl2_begin = 0.0;
    const float ampl2_end = 0.0;
    const float band1_error = 1.0;
    const float band2_error = 1.0;
    const int grid_density = 16;
    const float zero = 0.0;
    const int number_of_channels = 8;
    const int in_acquisition = 1;

    const float threshold = 0.01;
    const float doppler_max = 8000.0;
    const float doppler_step = 500.0;
    const int max_dwells = 1;
    const int tong_init_val = 2;
    const int tong_max_val = 10;
    const int tong_max_dwells = 30;
    const int coherent_integration_time_ms = 1;

    const float pll_bw_hz = 30.0;
    const float dll_bw_hz = 4.0;
    const float early_late_space_chips = 0.5;

    const int display_rate_ms = 500;
    const int output_rate_ms = 100;
    const int averaging_depth = 10;
};


void TtffTest::config_1()
{
    config = std::make_shared<InMemoryConfiguration>();

#if USE_GLOG_AND_GFLAGS
    config->set_property("GNSS-SDR.internal_fs_sps", std::to_string(FLAGS_fs_in));
#else
    config->set_property("GNSS-SDR.internal_fs_sps", std::to_string(absl::GetFlag(FLAGS_fs_in)));
#endif
    // Set the assistance system parameters
    config->set_property("GNSS-SDR.SUPL_gps_ephemeris_server", "supl.google.com");
    config->set_property("GNSS-SDR.SUPL_gps_ephemeris_port", std::to_string(7275));
    config->set_property("GNSS-SDR.SUPL_gps_acquisition_server", "supl.google.com");
    config->set_property("GNSS-SDR.SUPL_gps_acquisition_port", std::to_string(7275));
    config->set_property("GNSS-SDR.SUPL_MCC", std::to_string(244));
    config->set_property("GNSS-SDR.SUPL_MNC", std::to_string(5));
    config->set_property("GNSS-SDR.SUPL_LAC", "0x59e2");
    config->set_property("GNSS-SDR.SUPL_CI", "0x31b0");

    // Set the Signal Source
    config->set_property("SignalSource.item_type", "cshort");
    config->set_property("SignalSource.implementation", "UHD_Signal_Source");
    config->set_property("SignalSource.freq", std::to_string(central_freq));
    config->set_property("SignalSource.gain", std::to_string(gain_dB));
#if USE_GLOG_AND_GFLAGS
    config->set_property("SignalSource.sampling_frequency", std::to_string(FLAGS_fs_in));
    config->set_property("SignalSource.subdevice", FLAGS_subdevice);
    config->set_property("SignalSource.samples", std::to_string(FLAGS_fs_in * FLAGS_max_measurement_duration));
    config->set_property("SignalSource.device_address", FLAGS_device_address);
#else
    config->set_property("SignalSource.sampling_frequency", std::to_string(absl::GetFlag(FLAGS_fs_in)));
    config->set_property("SignalSource.subdevice", absl::GetFlag(FLAGS_subdevice));
    config->set_property("SignalSource.samples", std::to_string(absl::GetFlag(FLAGS_fs_in) * absl::GetFlag(FLAGS_max_measurement_duration)));
    config->set_property("SignalSource.device_address", absl::GetFlag(FLAGS_device_address));
#endif

    // Set the Signal Conditioner
    config->set_property("SignalConditioner.implementation", "Signal_Conditioner");
    config->set_property("DataTypeAdapter.implementation", "Pass_Through");
    config->set_property("DataTypeAdapter.item_type", "cshort");
    config->set_property("InputFilter.implementation", "Fir_Filter");
    config->set_property("InputFilter.dump", "false");
    config->set_property("InputFilter.input_item_type", "cshort");
    config->set_property("InputFilter.output_item_type", "gr_complex");
    config->set_property("InputFilter.taps_item_type", "float");
    config->set_property("InputFilter.number_of_taps", std::to_string(number_of_taps));
    config->set_property("InputFilter.number_of_bands", std::to_string(number_of_bands));
    config->set_property("InputFilter.band1_begin", std::to_string(band1_begin));
    config->set_property("InputFilter.band1_end", std::to_string(band1_end));
    config->set_property("InputFilter.band2_begin", std::to_string(band2_begin));
    config->set_property("InputFilter.band2_end", std::to_string(band2_end));
    config->set_property("InputFilter.ampl1_begin", std::to_string(ampl1_begin));
    config->set_property("InputFilter.ampl1_end", std::to_string(ampl1_end));
    config->set_property("InputFilter.ampl2_begin", std::to_string(ampl2_begin));
    config->set_property("InputFilter.ampl2_end", std::to_string(ampl2_end));
    config->set_property("InputFilter.band1_error", std::to_string(band1_error));
    config->set_property("InputFilter.band2_error", std::to_string(band2_error));
    config->set_property("InputFilter.filter_type", "bandpass");
    config->set_property("InputFilter.grid_density", std::to_string(grid_density));
#if USE_GLOG_AND_GFLAGS
    config->set_property("InputFilter.sampling_frequency", std::to_string(FLAGS_fs_in));
#else
    config->set_property("InputFilter.sampling_frequency", std::to_string(absl::GetFlag(FLAGS_fs_in)));
#endif
    config->set_property("InputFilter.IF", std::to_string(zero));
    config->set_property("Resampler.implementation", "Pass_Through");
    config->set_property("Resampler.dump", "false");
    config->set_property("Resampler.item_type", "gr_complex");
#if USE_GLOG_AND_GFLAGS
    config->set_property("Resampler.sample_freq_in", std::to_string(FLAGS_fs_in));
    config->set_property("Resampler.sample_freq_out", std::to_string(FLAGS_fs_in));
#else
    config->set_property("Resampler.sample_freq_in", std::to_string(absl::GetFlag(FLAGS_fs_in)));
    config->set_property("Resampler.sample_freq_out", std::to_string(absl::GetFlag(FLAGS_fs_in)));
#endif

    // Set the number of Channels
    config->set_property("Channels_1C.count", std::to_string(number_of_channels));
    config->set_property("Channels.in_acquisition", std::to_string(in_acquisition));
    config->set_property("Channel.signal", "1C");

    // Set Acquisition
    config->set_property("Acquisition_1C.implementation", "GPS_L1_CA_PCPS_Tong_Acquisition");
    config->set_property("Acquisition_1C.item_type", "gr_complex");
    config->set_property("Acquisition_1C.coherent_integration_time_ms", std::to_string(coherent_integration_time_ms));
    config->set_property("Acquisition_1C.threshold", std::to_string(threshold));
    config->set_property("Acquisition_1C.doppler_max", std::to_string(doppler_max));
    config->set_property("Acquisition_1C.doppler_step", std::to_string(doppler_step));
    config->set_property("Acquisition_1C.bit_transition_flag", "false");
    config->set_property("Acquisition_1C.max_dwells", std::to_string(max_dwells));
    config->set_property("Acquisition_1C.tong_init_val", std::to_string(tong_init_val));
    config->set_property("Acquisition_1C.tong_max_val", std::to_string(tong_max_val));
    config->set_property("Acquisition_1C.tong_max_dwells", std::to_string(tong_max_dwells));

    // Set Tracking
    config->set_property("Tracking_1C.implementation", "GPS_L1_CA_DLL_PLL_Tracking");
    config->set_property("Tracking_1C.item_type", "gr_complex");
    config->set_property("Tracking_1C.dump", "false");
    config->set_property("Tracking_1C.dump_filename", "./tracking_ch_");
    config->set_property("Tracking_1C.pll_bw_hz", std::to_string(pll_bw_hz));
    config->set_property("Tracking_1C.dll_bw_hz", std::to_string(dll_bw_hz));
    config->set_property("Tracking_1C.early_late_space_chips", std::to_string(early_late_space_chips));

    // Set Telemetry
    config->set_property("TelemetryDecoder_1C.implementation", "GPS_L1_CA_Telemetry_Decoder");
    config->set_property("TelemetryDecoder_1C.dump", "false");

    // Set Observables
    config->set_property("Observables.implementation", "Hybrid_Observables");
    config->set_property("Observables.dump", "false");
    config->set_property("Observables.dump_filename", "./observables.dat");

    // Set PVT
    config->set_property("PVT.implementation", "RTKLIB_PVT");
    config->set_property("PVT.positioning_mode", "Single");
    config->set_property("PVT.output_rate_ms", std::to_string(output_rate_ms));
    config->set_property("PVT.display_rate_ms", std::to_string(display_rate_ms));
    config->set_property("PVT.dump_filename", "./PVT");
    config->set_property("PVT.nmea_dump_filename", "./gnss_sdr_pvt.nmea");
    config->set_property("PVT.flag_nmea_tty_port", "false");
    config->set_property("PVT.nmea_dump_devname", "/dev/pts/4");
    config->set_property("PVT.flag_rtcm_server", "false");
    config->set_property("PVT.flag_rtcm_tty_port", "false");
    config->set_property("PVT.rtcm_dump_devname", "/dev/pts/1");
    config->set_property("PVT.dump", "false");
}


void TtffTest::config_2()
{
#if USE_GLOG_AND_GFLAGS
    if (FLAGS_config_file_ttff.empty())
        {
            std::string path = std::string(TEST_PATH);
            std::string filename = path + "../../conf/gnss-sdr_GPS_L1_USRP_X300_realtime.conf";
            config2 = std::make_shared<FileConfiguration>(filename);
        }
    else
        {
            config2 = std::make_shared<FileConfiguration>(FLAGS_config_file_ttff);
        }

    int d_sampling_rate;
    d_sampling_rate = config2->property("GNSS-SDR.internal_fs_sps", FLAGS_fs_in);
    config2->set_property("SignalSource.samples", std::to_string(d_sampling_rate * FLAGS_max_measurement_duration));
#else
    if (absl::GetFlag(FLAGS_config_file_ttff).empty())
        {
            std::string path = std::string(TEST_PATH);
            std::string filename = path + "../../conf/gnss-sdr_GPS_L1_USRP_X300_realtime.conf";
            config2 = std::make_shared<FileConfiguration>(filename);
        }
    else
        {
            config2 = std::make_shared<FileConfiguration>(absl::GetFlag(FLAGS_config_file_ttff));
        }

    int d_sampling_rate;
    d_sampling_rate = config2->property("GNSS-SDR.internal_fs_sps", absl::GetFlag(FLAGS_fs_in));
    config2->set_property("SignalSource.samples", std::to_string(d_sampling_rate * absl::GetFlag(FLAGS_max_measurement_duration)));
#endif
}


void receive_msg()
{
    bool leave = false;

    while (!leave)
        {
            // wait for the queues to be created
            const std::string queue_name = "gnss_sdr_ttff_message_queue";
            std::unique_ptr<boost::interprocess::message_queue> d_mq;
            bool queue_found = false;
            while (!queue_found)
                {
                    try
                        {
                            // Attempt to open the message queue
                            d_mq = std::make_unique<boost::interprocess::message_queue>(boost::interprocess::open_only, queue_name.c_str());
                            queue_found = true;  // Queue found
                        }
                    catch (const boost::interprocess::interprocess_exception &)
                        {
                            // Queue not found, wait and retry
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        }
                }

            double received_message;
            unsigned int priority;
            std::size_t received_size;
            // Receive a message (non-blocking)
            if (d_mq->try_receive(&received_message, sizeof(received_message), received_size, priority))
                {
                    // Validate the size of the received message
                    if (received_size == sizeof(double) && (received_message != 0) && (received_message != -1))
                        {
                            TTFF_v.push_back(received_message);
                            LOG(INFO) << "Valid Time-To-First-Fix: " << received_message << " [s]";
                            // Stop the receiver
                            double stop_message = -200.0;
                            const std::string queue_name_stop = "receiver_control_queue";
                            std::unique_ptr<boost::interprocess::message_queue> d_mq_stop;
                            bool queue_found2 = false;
                            while (!queue_found2)
                                {
                                    try
                                        {
                                            // Attempt to open the message queue
                                            d_mq_stop = std::make_unique<boost::interprocess::message_queue>(boost::interprocess::open_only, queue_name_stop.c_str());
                                            queue_found2 = true;  // Queue found
                                        }
                                    catch (const boost::interprocess::interprocess_exception &)
                                        {
                                            // Queue not found, wait and retry
                                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                        }
                                }
                            d_mq_stop->send(&stop_message, sizeof(stop_message), 0);
                        }
                    if (received_size == sizeof(double) && std::abs(received_message - (-1.0)) < 10 * std::numeric_limits<double>::epsilon())
                        {
                            leave = true;
                        }
                }
            else
                {
                    // No message available, add a small delay to prevent busy-waiting
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
        }
}


void TtffTest::print_TTFF_report(const std::vector<double> &ttff_v, const std::shared_ptr<ConfigurationInterface> &config_)
{
    std::ofstream ttff_report_file;
    std::string filename = "ttff_report";
    std::string filename_;

    boost::posix_time::ptime pt = boost::posix_time::second_clock::local_time();
    tm timeinfo = boost::posix_time::to_tm(pt);

    std::stringstream strm0;
    const int year = timeinfo.tm_year - 100;
    strm0 << year;
    const int month = timeinfo.tm_mon + 1;
    if (month < 10)
        {
            strm0 << "0";
        }
    strm0 << month;
    const int day = timeinfo.tm_mday;
    if (day < 10)
        {
            strm0 << "0";
        }
    strm0 << day << "_";
    const int hour = timeinfo.tm_hour;
    if (hour < 10)
        {
            strm0 << "0";
        }
    strm0 << hour;
    const int min = timeinfo.tm_min;

    if (min < 10)
        {
            strm0 << "0";
        }
    strm0 << min;
    const int sec = timeinfo.tm_sec;
    if (sec < 10)
        {
            strm0 << "0";
        }
    strm0 << sec;

    filename_ = filename + "_" + strm0.str() + ".txt";

    ttff_report_file.open(filename_.c_str());

    std::vector<double> ttff = ttff_v;
    bool read_ephemeris;
    bool false_bool = false;
    read_ephemeris = config_->property("GNSS-SDR.SUPL_read_gps_assistance_xml", false_bool);
    bool agnss;
    agnss = config_->property("GNSS-SDR.SUPL_gps_enabled", false_bool);
    double sum = std::accumulate(ttff.begin(), ttff.end(), 0.0);
    double mean = sum / ttff.size();
    double sq_sum = std::inner_product(ttff.begin(), ttff.end(), ttff.begin(), 0.0);
    double stdev = std::sqrt(sq_sum / ttff.size() - mean * mean);
    auto max_ttff = std::max_element(std::begin(ttff), std::end(ttff));
    auto min_ttff = std::min_element(std::begin(ttff), std::end(ttff));
    std::string source;
    std::string default_str = "default";
    source = config_->property("SignalSource.implementation", default_str);

    std::stringstream stm;

    stm << "---------------------------\n";
    stm << " Time-To-First-Fix Report\n";
    stm << "---------------------------\n";
    stm << "Initial receiver status: ";
    if (read_ephemeris)
        {
            stm << "Hot start.\n";
        }
    else
        {
            stm << "Cold start.\n";
        }
    stm << "A-GNSS: ";
    if (agnss && read_ephemeris)
        {
            stm << "Enabled.\n";
        }
    else
        {
            stm << "Disabled.\n";
        }
#if USE_GLOG_AND_GFLAGS
    stm << "Valid measurements (" << ttff.size() << "/" << FLAGS_num_measurements << "): ";
#else
    stm << "Valid measurements (" << ttff.size() << "/" << absl::GetFlag(FLAGS_num_measurements) << "): ";
#endif

    for (double ttff_ : ttff)
        {
            stm << ttff_ << " ";
        }
    stm << '\n';
    stm << "TTFF mean: " << mean << " [s]\n";
    if (!ttff.empty())
        {
            stm << "TTFF max: " << *max_ttff << " [s]\n";
            stm << "TTFF min: " << *min_ttff << " [s]\n";
        }
    stm << "TTFF stdev: " << stdev << " [s]\n";
    stm << "Operating System: " << std::string(HOST_SYSTEM) << '\n';
    stm << "Navigation mode: "
        << "3D\n";

    if (source != "UHD_Signal_Source")
        {
            stm << "Source: File\n";
        }
    else
        {
            stm << "Source: Live\n";
        }
    stm << "---------------------------\n";

    std::cout << stm.rdbuf();
    if (ttff_report_file.is_open())
        {
            ttff_report_file << stm.str();
            ttff_report_file.close();
        }
}


TEST_F(TtffTest /*unused*/, ColdStart /*unused*/)
{
    unsigned int num_measurements = 0;

    config_1();
    // Ensure Cold Start
    config->set_property("GNSS-SDR.SUPL_gps_enabled", "false");
    config->set_property("GNSS-SDR.SUPL_read_gps_assistance_xml", "false");

    config_2();
    // Ensure Cold Start
    config2->set_property("GNSS-SDR.SUPL_gps_enabled", "false");
    config2->set_property("GNSS-SDR.SUPL_read_gps_assistance_xml", "false");
    config2->set_property("PVT.flag_rtcm_server", "false");
#if USE_GLOG_AND_GFLAGS
    for (int n = 0; n < FLAGS_num_measurements; n++)
#else
    for (int n = 0; n < absl::GetFlag(FLAGS_num_measurements); n++)
#endif
        {
            // Create a new ControlThread object with a smart pointer
            std::shared_ptr<ControlThread> control_thread;
#if USE_GLOG_AND_GFLAGS
            if (FLAGS_config_file_ttff.empty())
#else
            if (absl::GetFlag(FLAGS_config_file_ttff).empty())
#endif
                {
                    control_thread = std::make_shared<ControlThread>(config);
                }
            else
                {
                    control_thread = std::make_shared<ControlThread>(config2);
                }

                // record startup time
#if USE_GLOG_AND_GFLAGS
            std::cout << "Starting measurement " << num_measurements + 1 << " / " << FLAGS_num_measurements << '\n';
#else
            std::cout << "Starting measurement " << num_measurements + 1 << " / " << absl::GetFlag(FLAGS_num_measurements) << '\n';
#endif
            std::chrono::time_point<std::chrono::system_clock> start;
            std::chrono::time_point<std::chrono::system_clock> end;
            start = std::chrono::system_clock::now();
            // start receiver
            try
                {
                    control_thread->run();
                }
            catch (const boost::exception &e)
                {
                    std::cout << "Boost exception: " << boost::diagnostic_information(e);
                }
            catch (const std::exception &ex)
                {
                    std::cout << "STD exception: " << ex.what();
                }

            // stop clock
            end = std::chrono::system_clock::now();
            std::chrono::duration<double> elapsed_seconds = end - start;
            double ttff = elapsed_seconds.count();

            std::shared_ptr<GNSSFlowgraph> flowgraph = control_thread->flowgraph();
            EXPECT_FALSE(flowgraph->running());

            num_measurements = num_measurements + 1;
            std::cout << "Just finished measurement " << num_measurements << ", which took " << ttff << " seconds.\n";
#if USE_GLOG_AND_GFLAGS
            if (n < FLAGS_num_measurements - 1)
#else
            if (n < absl::GetFlag(FLAGS_num_measurements) - 1)
#endif
                {
                    std::random_device r;
                    std::default_random_engine e1(r());
                    std::uniform_real_distribution<float> uniform_dist(0, 1);
                    float random_variable_0_1 = uniform_dist(e1);
                    int random_delay_s = static_cast<int>(random_variable_0_1 * 25.0);
                    std::cout << "Waiting a random amount of time (from 5 to 30 s) to start a new measurement... \n";
                    std::cout << "This time will wait " << random_delay_s + 5 << " s.\n"
                              << '\n';
                    std::this_thread::sleep_until(std::chrono::system_clock::now() + std::chrono::seconds(5) + std::chrono::seconds(random_delay_s));
                }
        }

        // Print TTFF report

#if USE_GLOG_AND_GFLAGS
    if (FLAGS_config_file_ttff.empty())
#else
    if (absl::GetFlag(FLAGS_config_file_ttff).empty())
#endif
        {
            print_TTFF_report(TTFF_v, config);
        }
    else
        {
            print_TTFF_report(TTFF_v, config2);
        }
    std::this_thread::sleep_until(std::chrono::system_clock::now() + std::chrono::seconds(5));  // let the USRP some time to rest before the next test
}


TEST_F(TtffTest /*unused*/, HotStart /*unused*/)
{
    unsigned int num_measurements = 0;
    TTFF_v.clear();

    config_1();
    // Ensure Hot Start
    config->set_property("GNSS-SDR.SUPL_gps_enabled", "true");
    config->set_property("GNSS-SDR.SUPL_read_gps_assistance_xml", "true");

    config_2();
    // Ensure Hot Start
    config2->set_property("GNSS-SDR.SUPL_gps_enabled", "true");
    config2->set_property("GNSS-SDR.SUPL_read_gps_assistance_xml", "true");
    config2->set_property("PVT.flag_rtcm_server", "false");

#if USE_GLOG_AND_GFLAGS
    for (int n = 0; n < FLAGS_num_measurements; n++)
#else
    for (int n = 0; n < absl::GetFlag(FLAGS_num_measurements); n++)
#endif
        {
            // Create a new ControlThread object with a smart pointer
            std::shared_ptr<ControlThread> control_thread;
#if USE_GLOG_AND_GFLAGS
            if (FLAGS_config_file_ttff.empty())
#else
            if (absl::GetFlag(FLAGS_config_file_ttff).empty())
#endif
                {
                    control_thread = std::make_shared<ControlThread>(config);
                }
            else
                {
                    control_thread = std::make_shared<ControlThread>(config2);
                }
                // record startup time
#if USE_GLOG_AND_GFLAGS
            std::cout << "Starting measurement " << num_measurements + 1 << " / " << FLAGS_num_measurements << '\n';
#else
            std::cout << "Starting measurement " << num_measurements + 1 << " / " << absl::GetFlag(FLAGS_num_measurements) << '\n';
#endif
            std::chrono::time_point<std::chrono::system_clock> start;
            std::chrono::time_point<std::chrono::system_clock> end;
            start = std::chrono::system_clock::now();

            // start receiver
            try
                {
                    control_thread->run();
                }
            catch (const boost::exception &e)
                {
                    std::cout << "Boost exception: " << boost::diagnostic_information(e);
                }
            catch (const std::exception &ex)
                {
                    std::cout << "STD exception: " << ex.what();
                }

            // stop clock
            end = std::chrono::system_clock::now();
            std::chrono::duration<double> elapsed_seconds = end - start;
            double ttff = elapsed_seconds.count();

            std::shared_ptr<GNSSFlowgraph> flowgraph = control_thread->flowgraph();
            EXPECT_FALSE(flowgraph->running());

            num_measurements = num_measurements + 1;
            std::cout << "Just finished measurement " << num_measurements << ", which took " << ttff << " seconds.\n";
#if USE_GLOG_AND_GFLAGS
            if (n < FLAGS_num_measurements - 1)
#else
            if (n < absl::GetFlag(FLAGS_num_measurements) - 1)
#endif
                {
                    std::random_device r;
                    std::default_random_engine e1(r());
                    std::uniform_real_distribution<float> uniform_dist(0, 1);
                    float random_variable_0_1 = uniform_dist(e1);
                    int random_delay_s = static_cast<int>(random_variable_0_1 * 25.0);
                    std::cout << "Waiting a random amount of time (from 5 to 30 s) to start new measurement... \n";
                    std::cout << "This time will wait " << random_delay_s + 5 << " s.\n"
                              << '\n';
                    std::this_thread::sleep_until(std::chrono::system_clock::now() + std::chrono::seconds(5) + std::chrono::seconds(random_delay_s));
                }
        }

        // Print TTFF report

#if USE_GLOG_AND_GFLAGS
    if (FLAGS_config_file_ttff.empty())
#else
    if (absl::GetFlag(FLAGS_config_file_ttff).empty())
#endif
        {
            print_TTFF_report(TTFF_v, config);
        }
    else
        {
            print_TTFF_report(TTFF_v, config2);
        }
}


int main(int argc, char **argv)
{
    std::cout << "Running Time-To-First-Fix test...\n";
    int res = 0;
    TTFF_v.clear();
    try
        {
            testing::InitGoogleTest(&argc, argv);
        }
    catch (...)
        {
        }  // catch the "testing::internal::<unnamed>::ClassUniqueToAlwaysTrue" from gtest

#if USE_GLOG_AND_GFLAGS
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);
#else
    absl::ParseCommandLine(argc, argv);
#endif

    // Start queue thread
    std::thread receive_msg_thread(receive_msg);

    // Run the Tests
    try
        {
            res = RUN_ALL_TESTS();
        }
    catch (...)
        {
            LOG(WARNING) << "Unexpected catch";
        }

    // Terminate the queue thread
    const std::string queue_name = "gnss_sdr_ttff_message_queue";
    boost::interprocess::message_queue::remove(queue_name.c_str());

    // Create a new message queue
    auto mq = std::make_unique<boost::interprocess::message_queue>(
        boost::interprocess::create_only,  // Create a new queue
        queue_name.c_str(),                // Queue name
        10,                                // Maximum number of messages
        sizeof(double)                     // Maximum message size
    );
    double finish = -1.0;
    if (mq)
        {
            mq->send(&finish, sizeof(finish), 0);
        }
    receive_msg_thread.join();
    boost::interprocess::message_queue::remove(queue_name.c_str());

#if USE_GLOG_AND_GFLAGS
    gflags::ShutDownCommandLineFlags();
#endif
    return res;
}
