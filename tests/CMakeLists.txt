

include_directories(BEFORE ${PROJECT_ROOT_DIR}/include)

add_executable(test-hpc-qrm-simple
    test-hpc-qrm-simple.cpp
    ${PROJECT_ROOT_DIR}/src/Daemon.cpp
    ${PROJECT_ROOT_DIR}/src/MQSSCompiler.cpp
)

target_compile_definitions(test-hpc-qrm-simple PRIVATE
      CUDAQ_QUAKE_PATH="${CUDAQ_QUAKE_PATH}"
      CUDAQ_TRANSLATE_PATH="${CUDAQ_TRANSLATE_PATH}"
      MQSS_CUDAQ_PATH="${MQSS_CUDAQ_PATH}"
      CUDAQ_OPT_PATH="${CUDAQ_OPT_PATH}"
      LOG_FILES_PATH="${LOG_FILES_PATH}"
)

target_link_libraries(
  "test-hpc-qrm-simple"
  PUBLIC rabbitmq
         mqss::mqss
         spdlog::spdlog
         nlohmann_json::nlohmann_json
      )