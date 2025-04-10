/*!
 * \file fft_length_test.cc
 * \brief  This file implements timing tests for the FFT.
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

#include "GPS_L1_CA.h"
#include "cuda_multicorrelator.h"
#include "gps_sdr_signal_replica.h"
#include <chrono>
#include <complex>
#include <cstdint>
#include <cuda.h>
#include <cuda_profiler_api.h>
#include <cuda_runtime.h>
#include <thread>

#if USE_GLOG_AND_GFLAGS
DEFINE_int32(gpu_multicorrelator_iterations_test, 1000, "Number of averaged iterations in GPU multicorrelator test timing test");
DEFINE_int32(gpu_multicorrelator_max_threads_test, 12, "Number of maximum concurrent correlators in GPU multicorrelator test timing test");
#else
ABSL_FLAG(int32_t, gpu_multicorrelator_iterations_test, 1000, "Number of averaged iterations in GPU multicorrelator test timing test");
ABSL_FLAG(int32_t, gpu_multicorrelator_max_threads_test, 12, "Number of maximum concurrent correlators in GPU multicorrelator test timing test");
#endif

void run_correlator_gpu(cuda_multicorrelator* correlator,
    float d_rem_carrier_phase_rad,
    float d_carrier_phase_step_rad,
    float d_code_phase_step_chips,
    float d_rem_code_phase_chips,
    int correlation_size,
    int d_n_correlator_taps)
{
#if USE_GLOG_AND_GFLAGS
    for (int k = 0; k < FLAGS_cpu_multicorrelator_iterations_test; k++)
#else
    for (int k = 0; k < absl::GetFlag(FLAGS_cpu_multicorrelator_iterations_test); k++)
#endif
        {
            correlator->Carrier_wipeoff_multicorrelator_resampler_cuda(d_rem_carrier_phase_rad,
                d_carrier_phase_step_rad,
                d_code_phase_step_chips,
                d_rem_code_phase_chips,
                correlation_size,
                d_n_correlator_taps);
        }
}


TEST(GpuMulticorrelatorTest, MeasureExecutionTime)
{
    std::chrono::time_point<std::chrono::system_clock> start, end;
    std::chrono::duration<double> elapsed_seconds(0);
#if USE_GLOG_AND_GFLAGS
    int max_threads = FLAGS_gpu_multicorrelator_max_threads_test;
#else
    int max_threads = absl::GetFlag(FLAGS_gpu_multicorrelator_max_threads_test);
#endif
    std::vector<std::thread> thread_pool;
    cuda_multicorrelator* correlator_pool[max_threads];
    unsigned int correlation_sizes[3] = {2048, 4096, 8192};
    double execution_times[3];

    gr_complex* d_ca_code;
    gr_complex* in_gpu;
    gr_complex* d_correlator_outs;

    int d_n_correlator_taps = 3;
    int d_vector_length = correlation_sizes[2];  // max correlation size to allocate all the necessary memory
    float* d_local_code_shift_chips;
    // Set GPU flags
    cudaSetDeviceFlags(cudaDeviceMapHost);
    // allocate host memory
    // pinned memory mode - use special function to get OS-pinned memory
    d_n_correlator_taps = 3;  // Early, Prompt, and Late
    // Get space for a vector with the C/A code replica sampled 1x/chip
    cudaHostAlloc(reinterpret_cast<void**>(&d_ca_code), (static_cast<int>(GPS_L1_CA_CODE_LENGTH_CHIPS) * sizeof(gr_complex)), cudaHostAllocMapped | cudaHostAllocWriteCombined);
    // Get space for the resampled early / prompt / late local replicas
    cudaHostAlloc(reinterpret_cast<void**>(&d_local_code_shift_chips), d_n_correlator_taps * sizeof(float), cudaHostAllocMapped | cudaHostAllocWriteCombined);
    cudaHostAlloc(reinterpret_cast<void**>(&in_gpu), 2 * d_vector_length * sizeof(gr_complex), cudaHostAllocMapped | cudaHostAllocWriteCombined);
    // correlator outputs (scalar)
    cudaHostAlloc(reinterpret_cast<void**>(&d_correlator_outs), sizeof(gr_complex) * d_n_correlator_taps, cudaHostAllocMapped | cudaHostAllocWriteCombined);

    // --- Perform initializations ------------------------------
    // local code resampler on GPU
    // generate local reference (1 sample per chip)
    gps_l1_ca_code_gen_complex(own::span<gr_complex>(d_ca_code, static_cast<int>(GPS_L1_CA_CODE_LENGTH_CHIPS)), 1, 0);
    // generate input signal
    for (int n = 0; n < 2 * d_vector_length; n++)
        {
            in_gpu[n] = std::complex<float>(static_cast<float>(rand()) / static_cast<float>(RAND_MAX), static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
        }
    // Set TAPs delay values [chips]
    float d_early_late_spc_chips = 0.5;
    d_local_code_shift_chips[0] = -d_early_late_spc_chips;
    d_local_code_shift_chips[1] = 0.0;
    d_local_code_shift_chips[2] = d_early_late_spc_chips;
    for (int n = 0; n < max_threads; n++)
        {
            correlator_pool[n] = new cuda_multicorrelator();
            correlator_pool[n]->init_cuda_integrated_resampler(d_vector_length, GPS_L1_CA_CODE_LENGTH_CHIPS, d_n_correlator_taps);
            correlator_pool[n]->set_input_output_vectors(d_correlator_outs, in_gpu);
        }

    float d_rem_carrier_phase_rad = 0.0;
    float d_carrier_phase_step_rad = 0.1;
    float d_code_phase_step_chips = 0.3;
    float d_rem_code_phase_chips = 0.4;

    EXPECT_NO_THROW(
        for (int correlation_sizes_idx = 0; correlation_sizes_idx < 3; correlation_sizes_idx++) {
            for (int current_max_threads = 1; current_max_threads < (max_threads + 1); current_max_threads++)
                {
                    std::cout << "Running " << current_max_threads << " concurrent correlators\n";
                    start = std::chrono::system_clock::now();
                    // create the concurrent correlator threads
                    for (int current_thread = 0; current_thread < current_max_threads; current_thread++)
                        {
                            // cudaProfilerStart();
                            thread_pool.push_back(std::thread(run_correlator_gpu,
                                correlator_pool[current_thread],
                                d_rem_carrier_phase_rad,
                                d_carrier_phase_step_rad,
                                d_code_phase_step_chips,
                                d_rem_code_phase_chips,
                                correlation_sizes[correlation_sizes_idx],
                                d_n_correlator_taps));
                            // cudaProfilerStop();
                        }
                    // wait the threads to finish they work and destroy the thread objects
                    for (auto& t : thread_pool)
                        {
                            t.join();
                        }
                    thread_pool.clear();
                    end = std::chrono::system_clock::now();
                    elapsed_seconds = end - start;
#if USE_GLOG_AND_GFLAGS
                    execution_times[correlation_sizes_idx] = elapsed_seconds.count() / static_cast<double>(FLAGS_gpu_multicorrelator_iterations_test);
#else
                    execution_times[correlation_sizes_idx] = elapsed_seconds.count() / static_cast<double>(absl::GetFlag(FLAGS_gpu_multicorrelator_iterations_test));
#endif
                    std::cout << "GPU Multicorrelator execution time for length=" << correlation_sizes[correlation_sizes_idx] << " : " << execution_times[correlation_sizes_idx] << " [s]\n";
                }
        });

    cudaFreeHost(in_gpu);
    cudaFreeHost(d_correlator_outs);
    cudaFreeHost(d_local_code_shift_chips);
    cudaFreeHost(d_ca_code);

    for (int n = 0; n < max_threads; n++)
        {
            correlator_pool[n]->free_cuda();
        }
}
