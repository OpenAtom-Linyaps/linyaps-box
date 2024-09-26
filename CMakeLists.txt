cmake_minimum_required(VERSION 3.0)

project(linglong-box)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(VERSION "1.0")

option(BUILD_STATIC "Build the static binary" ON)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  if (NOT CMAKE_DEBUG_ASAN)
    set(CMAKE_CXX_FLAGS_DEBUG "-O0  -Wall -g -ggdb3")
  else ()
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -fsanitize=undefined,address -Wall -g -ggdb3")
    set(BUILD_STATIC OFF)
  endif ()
endif ()

include(GNUInstallDirs)
include_directories(${linglong-box_SOURCE_DIR}/src)


find_package(PkgConfig)

add_subdirectory(src)

add_subdirectory(test)
