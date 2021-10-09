enable_testing()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")

find_package(GTest REQUIRED)
find_package(PkgConfig)

pkg_check_modules(SECCOMP REQUIRED libseccomp)

set(LINK_LIBS
    yaml-cpp
    GTest::GTest gtest_main
    seccomp
    stdc++)

add_executable(ll-test
               oci_test.cpp
               seccomp_test.cpp
               ../src/container/seccomp.cpp)

target_link_libraries(ll-test ${LINK_LIBS})

add_test(NAME ll_test COMMAND ll-test)
