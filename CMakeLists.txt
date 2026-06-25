

cmake_minimum_required(VERSION 3.22.1)
project(
  QuantumResourceManager
  VERSION 0.1
  DESCRIPTION "Quantum Resource Manager"
  LANGUAGES C CXX)


# Set the C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
set(CMAKE_CXX_FLAGS "-w -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-switch")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frtti -fexceptions")

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

find_package(qdmi REQUIRED)
find_package(qinfo REQUIRED)
# find_package(rabbitmq-c REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(MQSSCI REQUIRED)
find_package(MQSSDeploy REQUIRED)
find_package(Spdlog REQUIRED)
find_package(GTest REQUIRED)

# set(mqt-core_DIR "/workspaces/QRM/_deps/core/build/" 
#     CACHE PATH "Path to mqt-core CMake config")
# find_package(mqt-core REQUIRED)

# add_library(mqssci::mqssci STATIC IMPORTED GLOBAL)
# set_target_properties(mqssci::mqssci PROPERTIES
#   IMPORTED_LOCATION ${MQSSCI_INSTALL_DIR}/lib/libmoduleY.a
#   INTERFACE_INCLUDE_DIRECTORIES ${MQSSCI_INSTALL_DIR}/include
# )
# add_dependencies(mqssci::mqssci mqssci_external)


# find_package(Boost REQUIRED)
# message(STATUS "Boost version: ${Boost_VERSION}")
# if(NOT Boost_FOUND)
#   message(FATAL_ERROR "Boost library not found.")
# endif()
# find_package(BoostUUID REQUIRED)
# ##############################################################################

# Add the include paths
include_directories(
  BEFORE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  ${GTEST_INCLUDE_DIRS}
)

set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(CUDAQ_QUAKE_PATH "${MQSSCI_SRC_DIR}/_deps/mqss-cudaq/cudaq/bin/cudaq-quake")
set(CUDAQ_TRANSLATE_PATH "${MQSSCI_SRC_DIR}/_deps/mqss-cudaq/cudaq/bin/cudaq-translate")
set(CUDAQ_OPT_PATH "${MQSSCI_SRC_DIR}/_deps/mqss-cudaq/cudaq/bin/cudaq-opt")

set(MQSS_CUDAQ_PATH "${MQSSCI_INSTALL_DIR}/mqss-cudaq-opt")
set(MQSS_CATALYST_OPT "${MQSSCI_INSTALL_DIR}/mqss-catalyst-opt")

set(LOG_FILES_PATH "${LOGS_DIR}")

add_subdirectory(src)

add_executable("MQSSCompiler" src/MQSSCompiler.cpp)

set(COMPILER_QUEUE_STR "compiler.tasks.queue")
set(SCHEDULER_QUEUE_STR "scheduler.tasks.queue")
set(RESULTS_QUEUE_STR "test.results.queue")
set(SUBMITTER_QUEUE_STR "submitter.tasks.queue")

target_compile_definitions(MQSSCompiler PRIVATE
      CUDAQ_QUAKE_PATH="${CUDAQ_QUAKE_PATH}"
      CUDAQ_TRANSLATE_PATH="${CUDAQ_TRANSLATE_PATH}"
      MQSS_CUDAQ_PATH="${MQSS_CUDAQ_PATH}"
      CUDAQ_OPT_PATH="${CUDAQ_OPT_PATH}"
      COMPILER_LOG_FILES_PATH="${LOG_FILES_PATH}/Compiler.log"
      COMPILER_QUEUE="${COMPILER_QUEUE_STR}"
      SCHEDULER_QUEUE="${SCHEDULER_QUEUE_STR}"
      SUBMITTER_QUEUE="${SUBMITTER_QUEUE_STR}"
)

target_link_libraries(
  "MQSSCompiler"
    PUBLIC 
    rabbitmq
    mqss::mqss
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    Common
)

add_executable("MQSSScheduler" src/MQSSScheduler.cpp)

target_compile_definitions(MQSSScheduler PRIVATE
      SCHEDULER_LOG_FILES_PATH="${LOG_FILES_PATH}/Scheduler.log"
      COMPILER_QUEUE="${COMPILER_QUEUE_STR}"
      SCHEDULER_QUEUE="${SCHEDULER_QUEUE_STR}"
)

target_link_libraries(
  "MQSSScheduler"
  PUBLIC rabbitmq
         mqss::mqss
         spdlog::spdlog
         nlohmann_json::nlohmann_json
         Common
      )


add_executable("MQSSSubmitter" src/MQSSSubmitter.cpp src/Driver.cpp)

target_compile_definitions(MQSSSubmitter PRIVATE
      SUBMITTER_LOG_FILES_PATH="${LOG_FILES_PATH}/Submitter.log"
      SUBMITTER_QUEUE="${SUBMITTER_QUEUE_STR}"
      COMPILER_QUEUE="${COMPILER_QUEUE_STR}"
      RESULTS_QUEUE="${RESULTS_QUEUE_STR}"
)

target_include_directories("MQSSSubmitter" PRIVATE ${qdmi_SOURCE_DIR} )

target_link_libraries(
  "MQSSSubmitter"
  PUBLIC rabbitmq
         qdmi::qdmi
         qdmi::example_driver
         qdmi::qdmi_project_warnings
         qinfo
         mqss::mqss
         spdlog::spdlog
         GTest::gtest_main 
         GTest::gmock
         nlohmann_json::nlohmann_json
         Common
      )


add_subdirectory(tests)