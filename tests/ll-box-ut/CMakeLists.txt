set(linyaps-box_UNIT_TESTS ll-box-ut)
set(linyaps-box_UNIT_TESTS_SOURCE
    ./src/test.cpp ./src/semver_test.cpp ./src/span_test.cpp
    ./src/message_channel_test.cpp ./src/log_test.cpp)
set(linyaps-box_UNIT_TESTS_LINK_LIBRARIES PRIVATE "${linyaps-box_LIBRARY}")
set(linyaps-box_UNIT_TESTS_SOURCE_INCLUDE_DIRS
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

find_package(GTest CONFIG REQUIRED)

include(GoogleTest)

if(CMAKE_VERSION VERSION_LESS 3.20)
  add_library(GTest::gtest_main INTERFACE IMPORTED)
  target_link_libraries(GTest::gtest_main INTERFACE GTest::Main)
endif()

list(APPEND linyaps-box_UNIT_TESTS_LINK_LIBRARIES PRIVATE GTest::gtest_main)

add_executable("${linyaps-box_UNIT_TESTS}" ${linyaps-box_UNIT_TESTS_SOURCE})
target_include_directories("${linyaps-box_UNIT_TESTS}"
                           ${linyaps-box_UNIT_TESTS_SOURCE_INCLUDE_DIRS})
target_link_libraries("${linyaps-box_UNIT_TESTS}"
                      PRIVATE ${linyaps-box_UNIT_TESTS_LINK_LIBRARIES})
target_compile_options("${linyaps-box_UNIT_TESTS}"
                       PRIVATE -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.)

set(GTEST_DISCOVER_TESTS_ARGS
    "${linyaps-box_UNIT_TESTS}"
    WORKING_DIRECTORY
    "${CMAKE_CURRENT_BINARY_DIR}"
    DISCOVERY_MODE
    PRE_TEST
    PROPERTIES
    LABELS
    "unit")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  list(APPEND GTEST_DISCOVER_TESTS_ARGS ENVIRONMENT
       "LLVM_PROFILE_FILE=/dev/null")
endif()

gtest_discover_tests(${GTEST_DISCOVER_TESTS_ARGS})
