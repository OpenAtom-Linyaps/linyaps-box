set(linyaps-box_UNIT_TESTS ll-box-ut)
# find -regex '\./src/.*\.cpp' -type f -printf '%P\n' | sort
set(linyaps-box_UNIT_TESTS_SOURCE
    src/config/cpu_test.cpp
    src/config/hook_test.cpp
    src/config/io_priority_test.cpp
    src/config/linux_test.cpp
    src/config/memory_policy_test.cpp
    src/config/mount_test.cpp
    src/config/network_test.cpp
    src/config/ns_test.cpp
    src/config/oci_config_test.cpp
    src/config/process_test.cpp
    src/config/resources_test.cpp
    src/config/scheduler_test.cpp
    src/config/seccomp_test.cpp
    src/config/time_offset_test.cpp
    src/container_status_test.cpp
    src/date_test.cpp
    src/log_test.cpp
    src/main.cpp
    src/message_channel_test.cpp
    src/ns_flags_test.cpp
    src/parse_proc_stat_test.cpp
    src/ringbuffer_test.cpp
    src/rlimit_test.cpp
    src/semver_test.cpp
    src/span_test.cpp
    src/strict_json_test.cpp
    src/vfs_test.cpp
)

set(linyaps-box_UNIT_TESTS_LINK_LIBRARIES PRIVATE "${linyaps-box_LIBRARY}")
list(APPEND linyaps-box_UNIT_TESTS_LINK_LIBRARIES linyaps-box_build_flags)
set(linyaps-box_UNIT_TESTS_SOURCE_INCLUDE_DIRS
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# we need compatible with gtest 1.8.1+
find_package(GTest REQUIRED)

include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_INCLUDES ${GTest_INCLUDE_DIRS})
check_cxx_source_compiles(
    "#include <gtest/gtest.h>
   int main() {
#ifdef INSTANTIATE_TEST_SUITE_P
   return 0;
#else
#  error INSTANTIATE_TEST_SUITE_P not defined
#endif
  }"
    GTEST_HAS_INSTANTIATE_TEST_SUITE_P
)

if(TARGET GTest::gmock)
    list(APPEND linyaps-box_UNIT_TESTS_LINK_LIBRARIES GTest::gmock)
else()
    # fallback
    find_path(GMOCK_INCLUDE_DIR gmock/gmock.h)
    find_library(GMOCK_LIBRARY NAMES gmock gmockd)

    if(GMOCK_INCLUDE_DIR AND GMOCK_LIBRARY)
        add_library(GTest::gmock UNKNOWN IMPORTED)
        set_target_properties(
            GTest::gmock
            PROPERTIES
                IMPORTED_LOCATION "${GMOCK_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${GMOCK_INCLUDE_DIR}"
        )

        if(TARGET GTest::gtest)
            set_property(
                TARGET GTest::gmock
                PROPERTY INTERFACE_LINK_LIBRARIES GTest::gtest
            )
        endif()

        list(APPEND linyaps-box_UNIT_TESTS_LINK_LIBRARIES GTest::gmock)
    else()
        message(FATAL_ERROR "gmock not found but required by unit tests")
    endif()
endif()

include(GoogleTest)

list(APPEND linyaps-box_UNIT_TESTS_LINK_LIBRARIES GTest::gtest)

add_executable("${linyaps-box_UNIT_TESTS}" ${linyaps-box_UNIT_TESTS_SOURCE})

target_include_directories(
    "${linyaps-box_UNIT_TESTS}"
    ${linyaps-box_UNIT_TESTS_SOURCE_INCLUDE_DIRS}
)

target_link_libraries(
    "${linyaps-box_UNIT_TESTS}"
    PRIVATE ${linyaps-box_UNIT_TESTS_LINK_LIBRARIES}
)

# Fixtures referenced by relative paths (e.g. "data/oci.json") resolve against
# WORKING_DIRECTORY below (this binary dir). Link the shared tests/data tree
# instead of copying it: edits and deletions are reflected without stale files
# and without a copy on every build.
file(
    CREATE_LINK
        "${PROJECT_SOURCE_DIR}/tests/data"
        "${CMAKE_CURRENT_BINARY_DIR}/data"
    SYMBOLIC
)

set_property(TARGET "${linyaps-box_UNIT_TESTS}" PROPERTY CXX_STANDARD 17)
set_property(TARGET "${linyaps-box_UNIT_TESTS}" PROPERTY CXX_EXTENSIONS OFF)
set_property(
    TARGET "${linyaps-box_UNIT_TESTS}"
    PROPERTY CXX_STANDARD_REQUIRED ON
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(
        APPEND GTEST_DISCOVER_TESTS_ARGS
        ENVIRONMENT
        "LLVM_PROFILE_FILE=/dev/null"
    )
endif()

if(NOT GTEST_HAS_INSTANTIATE_TEST_SUITE_P)
    target_compile_definitions(
        "${linyaps-box_UNIT_TESTS}"
        PRIVATE INSTANTIATE_TEST_SUITE_P=INSTANTIATE_TEST_CASE_P
    )
endif()

if(linyaps-box_ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # gtest_discover_tests registers every test as its own ctest process
    list(
        APPEND TEST_ENV_VARS
        "LLVM_PROFILE_FILE=${CMAKE_CURRENT_BINARY_DIR}/llvm-%p.profraw"
    )
endif()

gtest_discover_tests(
    "${linyaps-box_UNIT_TESTS}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "unit" ENVIRONMENT "${TEST_ENV_VARS}"
)

if(linyaps-box_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        find_program(LLVM_PROFDATA llvm-profdata REQUIRED)
        find_program(LLVM_COV llvm-cov REQUIRED)

        set(_coverage_ignore_regex "(^|/)(tests|_deps|external)/")

        add_custom_target(
            coverage-ut-run
            # Drop stale profiles first
            COMMAND
                bash -c
                "find '${CMAKE_CURRENT_BINARY_DIR}' -name 'llvm-*.profraw' -delete"
            COMMAND "${CMAKE_CTEST_COMMAND}" -L unit --output-on-failure
            COMMAND
                "${CMAKE_COMMAND}" "-DPROFILE_DIR=${CMAKE_CURRENT_BINARY_DIR}"
                "-DPROF_OUT=${CMAKE_CURRENT_BINARY_DIR}/coverage-ut.profdata"
                "-DLLVM_PROFDATA=${LLVM_PROFDATA}" -P
                "${CMAKE_CURRENT_SOURCE_DIR}/../merge-llvm-profiles.cmake"
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            DEPENDS "${linyaps-box_UNIT_TESTS}"
            VERBATIM
        )

        add_custom_target(
            coverage-ut
            COMMAND
                "${CMAKE_COMMAND}" -E chdir "${PROJECT_SOURCE_DIR}"
                "${LLVM_COV}" report "$<TARGET_FILE:${linyaps-box_UNIT_TESTS}>"
                "-instr-profile=${CMAKE_CURRENT_BINARY_DIR}/coverage-ut.profdata"
                "-ignore-filename-regex=${_coverage_ignore_regex}"
            DEPENDS coverage-ut-run
            VERBATIM
        )

        add_custom_target(
            coverage-lcov
            COMMAND
                "${LLVM_COV}" export -format=lcov
                "$<TARGET_FILE:${linyaps-box_UNIT_TESTS}>"
                "-instr-profile=${CMAKE_CURRENT_BINARY_DIR}/coverage-ut.profdata"
                "-ignore-filename-regex=${_coverage_ignore_regex}" >
                "${CMAKE_CURRENT_BINARY_DIR}/coverage.info"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            DEPENDS coverage-ut-run
            VERBATIM
        )

        add_custom_target(
            coverage-html
            COMMAND
                "${CMAKE_COMMAND}" -E rm -rf
                "${CMAKE_CURRENT_BINARY_DIR}/coverage-html"
            COMMAND
                "${CMAKE_COMMAND}" -E chdir "${PROJECT_SOURCE_DIR}"
                "${LLVM_COV}" show "$<TARGET_FILE:${linyaps-box_UNIT_TESTS}>"
                "-instr-profile=${CMAKE_CURRENT_BINARY_DIR}/coverage-ut.profdata"
                "-ignore-filename-regex=${_coverage_ignore_regex}" -format=html
                "-output-dir=${CMAKE_CURRENT_BINARY_DIR}/coverage-html"
            DEPENDS coverage-ut-run
            VERBATIM
        )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        find_program(GCOVR gcovr REQUIRED)

        set(_gcovr_args
            -r
            .
            --object-directory
            "${CMAKE_BINARY_DIR}/CMakeFiles/linyaps-box.dir"
            --object-directory
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${linyaps-box_UNIT_TESTS}.dir"
            --filter
            "src/linyaps_box"
        )

        add_custom_target(
            coverage-ut-run
            COMMAND bash -c "find '${CMAKE_BINARY_DIR}' -name '*.gcda' -delete"
            COMMAND
                bash -c
                "find '${CMAKE_BINARY_DIR}' -name '*.gcov.json.gz' -delete"
            COMMAND
                "${CMAKE_COMMAND}" -E chdir "${CMAKE_CURRENT_BINARY_DIR}"
                "${CMAKE_CTEST_COMMAND}" -L unit --output-on-failure
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            DEPENDS "${linyaps-box_UNIT_TESTS}"
        )

        add_custom_target(
            coverage-gcovr
            COMMAND
                "${CMAKE_COMMAND}" -E rm -rf
                "${CMAKE_CURRENT_BINARY_DIR}/coverage-html"
            COMMAND
                "${CMAKE_COMMAND}" -E make_directory
                "${CMAKE_CURRENT_BINARY_DIR}/coverage-html"
            COMMAND
                "${GCOVR}" ${_gcovr_args} --txt --lcov
                "${CMAKE_CURRENT_BINARY_DIR}/coverage.info" --html-details
                "${CMAKE_CURRENT_BINARY_DIR}/coverage-html/index.html"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            DEPENDS coverage-ut-run
        )

        add_custom_target(coverage-ut DEPENDS coverage-gcovr)
        add_custom_target(coverage-lcov DEPENDS coverage-gcovr)
        add_custom_target(coverage-html DEPENDS coverage-gcovr)
    endif()
endif()
