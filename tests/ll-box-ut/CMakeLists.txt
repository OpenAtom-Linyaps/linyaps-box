include(GoogleTest)

set(linyaps-box_UNIT_TESTS ll-box-ut)
set(linyaps-box_UNIT_TESTS_SOURCE ./src/test.cpp)
set(linyaps-box_UNIT_TESTS_LINK_LIBRARIES PRIVATE "${linyaps-box_LIBRARY}")
set(linyaps-box_UNIT_TESTS_SOURCE_INCLUDE_DIRS
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

find_package(GTest REQUIRED)

if(CMAKE_VERSION VERSION_LESS 3.20)
  add_library(GTest::gtest_main INTERFACE IMPORTED)
  target_link_libraries(GTest::gtest_main INTERFACE GTest::Main)
endif()

list(APPEND linyaps-box_UNIT_TESTS_LINK_LIBRARIES PRIVATE GTest::gtest_main)

add_executable("${linyaps-box_UNIT_TESTS}" ${linyaps-box_UNIT_TESTS_SOURCE})
target_include_directories("${linyaps-box_UNIT_TESTS}"
                           ${linyaps-box_UNIT_TESTS_SOURCE_INCLUDE_DIRS})
target_link_libraries("${linyaps-box_UNIT_TESTS}"
                      ${linyaps-box_UNIT_TESTS_LINK_LIBRARIES})
target_compile_features("${linyaps-box_UNIT_TESTS}" PRIVATE cxx_std_17)
set_property(TARGET "${linyaps-box_UNIT_TESTS}" PROPERTY CXX_STANDARD 17)
set_property(TARGET "${linyaps-box_UNIT_TESTS}" PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET "${linyaps-box_UNIT_TESTS}" PROPERTY CXX_STANDARD_REQUIRED
                                                         ON)
target_compile_options("${linyaps-box_UNIT_TESTS}"
                       PRIVATE -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.)

set(GTEST_DISCOVER_TESTS_ARGS "${linyaps-box_UNIT_TESTS}" WORKING_DIRECTORY
                              "${CMAKE_CURRENT_BINARY_DIR}")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  list(APPEND GTEST_DISCOVER_TESTS_ARGS PROPERTIES ENVIRONMENT
       "LLVM_PROFILE_FILE=/dev/null")
endif()

gtest_discover_tests(${GTEST_DISCOVER_TESTS_ARGS})
