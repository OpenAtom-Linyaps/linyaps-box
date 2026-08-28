set(linyaps-box_UNIT_TESTS ll-box-ut)
set(linyaps-box_UNIT_TESTS_SOURCE
    ./src/main.cpp
    ./src/test.cpp
    ./src/semver_test.cpp
    ./src/span_test.cpp
    ./src/message_channel_test.cpp
    ./src/log_test.cpp
    ./src/vfs_test.cpp
    ./src/parse_proc_stat_test.cpp
    ./src/container_status_test.cpp)
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

include(GoogleTest)

list(APPEND linyaps-box_UNIT_TESTS_LINK_LIBRARIES GTest::gtest)

add_executable("${linyaps-box_UNIT_TESTS}" ${linyaps-box_UNIT_TESTS_SOURCE})

target_include_directories("${linyaps-box_UNIT_TESTS}"
                           ${linyaps-box_UNIT_TESTS_SOURCE_INCLUDE_DIRS})

target_link_libraries("${linyaps-box_UNIT_TESTS}"
                      PRIVATE ${linyaps-box_UNIT_TESTS_LINK_LIBRARIES})

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
