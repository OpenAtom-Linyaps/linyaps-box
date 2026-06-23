add_library(zeus_expected INTERFACE)
add_library(zeus::expected ALIAS zeus_expected)
target_include_directories(zeus_expected
                           INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include/")
