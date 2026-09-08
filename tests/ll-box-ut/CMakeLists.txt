set(linyaps-box_UNIT_TESTS ll-box-ut)
set(linyaps-box_UNIT_TESTS_SOURCE
    ./src/main.cpp
    ./src/semver_test.cpp
    ./src/span_test.cpp
    ./src/message_channel_test.cpp
    ./src/ns_flags_test.cpp
    ./src/rlimit_test.cpp
    ./src/log_test.cpp
    ./src/vfs_test.cpp
    ./src/parse_proc_stat_test.cpp
    ./src/date_test.cpp
    ./src/container_status_test.cpp
    ./src/config/cpu_test.cpp
    ./src/config/hook_test.cpp
    ./src/config/io_priority_test.cpp
    ./src/config/linux_test.cpp
    ./src/config/memory_policy_test.cpp
    ./src/config/mount_test.cpp
    ./src/config/network_test.cpp
    ./src/config/ns_test.cpp
    ./src/config/oci_config_test.cpp
    ./src/config/process_test.cpp
    ./src/config/resources_test.cpp
    ./src/config/scheduler_test.cpp
    ./src/config/seccomp_test.cpp)
set(linyaps-box_UNIT_TESTS_LINK_LIBRARIES PRIVATE "${linyaps-box_LIBRARY}")
set(linyaps-box_UNIT_TESTS_SOURCE_INCLUDE_DIRS
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

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
  GTEST_HAS_INSTANTIATE_TEST_SUITE_P)

# gmock provides the matcher library (EXPECT_THAT/ASSERT_THAT), which has been
# part of the googletest repo since 1.8.0 — so the 1.8.1 floor already has it.
# The Config-mode GTest package exports GTest::gmock when gmock is installed;
# fall back to a raw -lgmock lookup on older distros that split the package.
check_cxx_source_compiles("#include <gmock/gmock.h>
   int main() { return 0; }" GTEST_HAS_GMOCK_HEADER)

if(TARGET GTest::gmock AND GTEST_HAS_GMOCK_HEADER)
  list(APPEND linyaps-box_UNIT_TESTS_LINK_LIBRARIES GTest::gmock)
elseif(GTEST_HAS_GMOCK_HEADER)
  find_library(GMOCK_LIBRARY NAMES gmock gmockd REQUIRED)
  list(APPEND linyaps-box_UNIT_TESTS_LINK_LIBRARIES ${GMOCK_LIBRARY})
endif()

include(GoogleTest)

list(APPEND linyaps-box_UNIT_TESTS_LINK_LIBRARIES GTest::gtest)

add_executable("${linyaps-box_UNIT_TESTS}" ${linyaps-box_UNIT_TESTS_SOURCE})

target_include_directories("${linyaps-box_UNIT_TESTS}"
                           ${linyaps-box_UNIT_TESTS_SOURCE_INCLUDE_DIRS})

target_link_libraries("${linyaps-box_UNIT_TESTS}"
                      PRIVATE ${linyaps-box_UNIT_TESTS_LINK_LIBRARIES})

# Test fixtures referenced by relative paths (e.g. "data/oci.json") resolve
# against WORKING_DIRECTORY below; stage the data/ tree into the binary dir.
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/data"
     DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

target_compile_options("${linyaps-box_UNIT_TESTS}"
                       PRIVATE -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.)

set_property(TARGET "${linyaps-box_UNIT_TESTS}" PROPERTY CXX_STANDARD 17)
set_property(TARGET "${linyaps-box_UNIT_TESTS}" PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET "${linyaps-box_UNIT_TESTS}" PROPERTY CXX_STANDARD_REQUIRED
                                                         ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  list(APPEND GTEST_DISCOVER_TESTS_ARGS ENVIRONMENT
       "LLVM_PROFILE_FILE=/dev/null")
endif()

if(NOT GTEST_HAS_INSTANTIATE_TEST_SUITE_P)
  target_compile_definitions(
    "${linyaps-box_UNIT_TESTS}"
    PRIVATE INSTANTIATE_TEST_SUITE_P=INSTANTIATE_TEST_CASE_P)
endif()

gtest_discover_tests(
  "${linyaps-box_UNIT_TESTS}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" DISCOVERY_MODE PRE_TEST
  PROPERTIES LABELS "unit" ENVIRONMENT "${TEST_ENV_VARS}")
