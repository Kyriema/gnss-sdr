# GNSS-SDR is a Global Navigation Satellite System software-defined receiver.
# This file is part of GNSS-SDR.
#
# SPDX-FileCopyrightText: 2010-2020 C. Fernandez-Prades cfernandez(at)cttc.es
# SPDX-License-Identifier: BSD-3-Clause


add_subdirectory(libswiftcnav)

set(TELEMETRY_DECODER_LIB_SOURCES
    tlm_conf.cc
    tlm_crc_stats.cc
    tlm_utils.cc
    viterbi_decoder.cc
    viterbi_decoder_sbas.cc
)

set(TELEMETRY_DECODER_LIB_HEADERS

    tlm_conf.h
    tlm_crc_stats.h
    tlm_utils.h
    viterbi_decoder.h
    viterbi_decoder_sbas.h
)

list(SORT TELEMETRY_DECODER_LIB_HEADERS)
list(SORT TELEMETRY_DECODER_LIB_SOURCES)

if(USE_CMAKE_TARGET_SOURCES)
    add_library(telemetry_decoder_libs STATIC)
    target_sources(telemetry_decoder_libs
        PRIVATE
            ${TELEMETRY_DECODER_LIB_SOURCES}
        PUBLIC
            ${TELEMETRY_DECODER_LIB_HEADERS}
    )
else()
    source_group(Headers FILES ${TELEMETRY_DECODER_LIB_HEADERS})
    add_library(telemetry_decoder_libs
        ${TELEMETRY_DECODER_LIB_SOURCES}
        ${TELEMETRY_DECODER_LIB_HEADERS}
    )
endif()

target_link_libraries(telemetry_decoder_libs
    PRIVATE
        Volkgnsssdr::volkgnsssdr
        algorithms_libs
        Matio::matio
)

if(ENABLE_GLOG_AND_GFLAGS)
    target_link_libraries(telemetry_decoder_libs PRIVATE Gflags::gflags Glog::glog)
    target_compile_definitions(telemetry_decoder_libs PRIVATE -DUSE_GLOG_AND_GFLAGS=1)
else()
    target_link_libraries(telemetry_decoder_libs PRIVATE absl::flags absl::log)
endif()

target_include_directories(telemetry_decoder_libs
    PUBLIC
        ${GNSSSDR_SOURCE_DIR}/src/core/interfaces
)

if(ENABLE_CLANG_TIDY)
    if(CLANG_TIDY_EXE)
        set_target_properties(telemetry_decoder_libs
            PROPERTIES
                CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
        )
    endif()
endif()

set_property(TARGET telemetry_decoder_libs
    APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
