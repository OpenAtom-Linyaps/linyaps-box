cmake_minimum_required(VERSION 3.0)

set(DVERSION "1.0" CACHE STRING "define project version")

project(linglong-box)

project(linglong-box
  VERSION ${DVERSION}
  DESCRIPTION "linglong container"
  HOMEPAGE_URL "https://github.com/linuxdeepin/linglong-box"
  LANGUAGES CXX C
)

include(GNUInstallDirs)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX /usr)
endif()

# 保证项目兼容性
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# set(VERSION "1.0")
option(BUILD_STATIC "Build the static binary" OFF)

# configure debug mode and asan
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose Release or Debug" FORCE)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--as-needed")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
endif()

include_directories(${linglong-box_SOURCE_DIR})
include_directories(${linglong-box_SOURCE_DIR}/src)

find_package(PkgConfig)

add_subdirectory(src)

add_subdirectory(tests)
