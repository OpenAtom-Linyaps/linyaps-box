# https://github.com/nlohmann/json/tree/v3.11.3?tab=readme-ov-file#integration

add_library(nlohmann_json INTERFACE)
add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
target_include_directories(nlohmann_json INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include/")
