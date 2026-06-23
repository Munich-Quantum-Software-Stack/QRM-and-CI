

# find_package(nlohmann_json REQUIRED)
# find_package(MQSSDeploy REQUIRED)
# find_package(Spdlog REQUIRED)


add_executable(test-hpc-qrm-simple
    test-hpc-qrm-simple.cpp
)

target_include_directories(test-hpc-qrm-simple PRIVATE
    ${PROJECT_ROOT_DIR}/include
)

target_compile_definitions(test-hpc-qrm-simple PRIVATE
      TEST_CASE_LOG_FILES_PATH="${LOG_FILES_PATH}/Test-Case.log"
      SCHEDULER_QUEUE="${SCHEDULER_QUEUE_STR}"
      RESULTS_QUEUE="${RESULTS_QUEUE_STR}"
)


target_link_libraries(test-hpc-qrm-simple
    PUBLIC 
    rabbitmq
    mqss::mqss
    spdlog::spdlog
    GTest::gtest_main 
    GTest::gmock
    nlohmann_json::nlohmann_json
    Common
)