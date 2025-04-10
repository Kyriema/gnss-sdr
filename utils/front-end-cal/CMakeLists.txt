# GNSS-SDR is a Global Navigation Satellite System software-defined receiver.
# This file is part of GNSS-SDR.
#
# SPDX-FileCopyrightText: 2010-2025 C. Fernandez-Prades cfernandez(at)cttc.es
# SPDX-License-Identifier: BSD-3-Clause


if(USE_CMAKE_TARGET_SOURCES)
    add_library(front_end_cal_lib STATIC)
    target_sources(front_end_cal_lib
        PRIVATE
            front_end_cal.cc
        PUBLIC
            front_end_cal.h
    )
else()
    source_group(Headers FILES front_end_cal.h)
    add_library(front_end_cal_lib front_end_cal.cc front_end_cal.h)
endif()

target_link_libraries(front_end_cal_lib
    PUBLIC
        Armadillo::armadillo
        Threads::Threads
        acquisition_adapters
        gnss_sdr_flags
        channel_libs
        algorithms_libs
        core_receiver
        core_libs
    PRIVATE
        Boost::headers
        Gnuradio::blocks
        Gnuradio::runtime
)

if(ENABLE_GLOG_AND_GFLAGS)
    target_link_libraries(front_end_cal_lib PRIVATE Gflags::gflags Glog::glog)
    target_compile_definitions(front_end_cal_lib PRIVATE -DUSE_GLOG_AND_GFLAGS=1)
else()
    target_link_libraries(front_end_cal_lib PRIVATE absl::flags absl::log)
endif()

if(ENABLE_CLANG_TIDY)
    if(CLANG_TIDY_EXE)
        set_target_properties(front_end_cal_lib
            PROPERTIES
                CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
        )
    endif()
endif()

if(USE_CMAKE_TARGET_SOURCES)
    add_executable(front-end-cal)
    target_sources(front-end-cal PRIVATE main.cc)
else()
    add_executable(front-end-cal main.cc)
endif()

target_link_libraries(front-end-cal
    PRIVATE
        core_receiver
        algorithms_libs
        front_end_cal_lib
        gnss_sdr_flags
        Boost::headers
)

if(ENABLE_GLOG_AND_GFLAGS)
    target_link_libraries(front-end-cal PRIVATE Glog::glog)
    target_compile_definitions(front-end-cal PRIVATE -DUSE_GLOG_AND_GFLAGS=1)
else()
    target_link_libraries(front-end-cal PRIVATE absl::flags absl::flags_parse absl::log absl::log_initialize absl::log_sink absl::log_sink_registry)
endif()

if(GNURADIO_USES_STD_POINTERS)
    target_compile_definitions(front-end-cal
        PRIVATE -DGNURADIO_USES_STD_POINTERS=1
    )
endif()

target_compile_definitions(front-end-cal
    PRIVATE -DGNSS_SDR_VERSION="${VERSION}"
    PRIVATE -DGNSSSDR_INSTALL_DIR="${CMAKE_INSTALL_PREFIX}"
)

if(USE_GENERIC_LAMBDAS)
    set(has_generic_lambdas HAS_GENERIC_LAMBDA=1)
    set(no_has_generic_lambdas HAS_GENERIC_LAMBDA=0)
    target_compile_definitions(front-end-cal
        PRIVATE
            "$<$<COMPILE_FEATURES:cxx_generic_lambdas>:${has_generic_lambdas}>"
            "$<$<NOT:$<COMPILE_FEATURES:cxx_generic_lambdas>>:${no_has_generic_lambdas}>"
    )
else()
    target_compile_definitions(front-end-cal
        PRIVATE
            -DHAS_GENERIC_LAMBDA=0
    )
endif()

if(USE_BOOST_BIND_PLACEHOLDERS)
    target_compile_definitions(front-end-cal
        PRIVATE
            -DUSE_BOOST_BIND_PLACEHOLDERS=1
    )
endif()

if(PMT_USES_BOOST_ANY)
    target_compile_definitions(front-end-cal
        PRIVATE
            -DPMT_USES_BOOST_ANY=1
    )
endif()

include(XcodeRemoveWarningDuplicates)
xcode_remove_warning_duplicates(front-end-cal)

if(ENABLE_STRIP)
    set_target_properties(front-end-cal PROPERTIES LINK_FLAGS "-s")
endif()

if(ENABLE_CLANG_TIDY)
    if(CLANG_TIDY_EXE)
        set_target_properties(front-end-cal
            PROPERTIES
                CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
        )
    endif()
endif()

add_custom_command(TARGET front-end-cal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:front-end-cal>
        ${LOCAL_INSTALL_BASE_DIR}/install/$<TARGET_FILE_NAME:front-end-cal>
)

install(TARGETS front-end-cal
    RUNTIME DESTINATION bin
    COMPONENT "front-end-cal"
)

find_program(GZIP gzip
    ${GNSSSDR_BIN_PATHS}
)

if(NOT GZIP_NOTFOUND)
    execute_process(COMMAND gzip -9 -c ${GNSSSDR_SOURCE_DIR}/docs/manpage/front-end-cal-manpage
        WORKING_DIRECTORY ${GNSSSDR_BINARY_DIR} OUTPUT_FILE "${GNSSSDR_BINARY_DIR}/front-end-cal.1.gz"
    )
    install(FILES ${GNSSSDR_BINARY_DIR}/front-end-cal.1.gz DESTINATION share/man/man1)
endif()
