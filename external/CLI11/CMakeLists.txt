add_library(CLI11 INTERFACE)
add_library(CLI11::CLI11 ALIAS CLI11)
target_include_directories(CLI11 INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include/")
