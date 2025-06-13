if(TARGET nlohmann_json::nlohmann_json)
  set(nlohmann_json_FOUND TRUE)

  # Set version variables
  set(nlohmann_json_VERSION_MAJOR 3)
  set(nlohmann_json_VERSION_MINOR 12)
  set(nlohmann_json_VERSION_PATCH 0)
  set(nlohmann_json_VERSION "3.12.0")
endif()
