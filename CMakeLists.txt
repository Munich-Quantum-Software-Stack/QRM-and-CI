

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
# find_package(rabbitmq-c REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(MQSSCI REQUIRED)
find_package(MQSSDeploy REQUIRED)
find_package(Spdlog REQUIRED)

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
  ${MQT_QMAP_INCLUDE_DIR}
  ${MQT_CORE_INCLUDE_DIR}
  ${QDMI_INCLUDE_DIR}
  ${QDMI_EXAMPLES_DIR}
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_subdirectory(src)

add_executable("MQSSCompiler" src/MQSSCompiler.cpp)

set(CUDAQ_QUAKE_PATH "${MQSSCI_SRC_DIR}/_deps/mqss-cudaq/cudaq/bin/cudaq-quake")
set(CUDAQ_TRANSLATE_PATH "${MQSSCI_SRC_DIR}/_deps/mqss-cudaq/cudaq/bin/cudaq-translate")
set(CUDAQ_OPT_PATH "${MQSSCI_SRC_DIR}/_deps/mqss-cudaq/cudaq/bin/cudaq-opt")

set(MQSS_CUDAQ_PATH "${MQSSCI_INSTALL_DIR}/mqss-cudaq-opt")
set(MQSS_CATALYST_OPT "${MQSSCI_INSTALL_DIR}/mqss-catalyst-opt")

set(LOG_FILES_PATH "${MQSSCI_SRC_DIR}/build/logs")

target_compile_definitions(MQSSCompiler PRIVATE
      CUDAQ_QUAKE_PATH="${CUDAQ_QUAKE_PATH}"
      CUDAQ_TRANSLATE_PATH="${CUDAQ_TRANSLATE_PATH}"
      MQSS_CUDAQ_PATH="${MQSS_CUDAQ_PATH}"
      CUDAQ_OPT_PATH="${CUDAQ_OPT_PATH}"
      LOG_FILES_PATH="${LOG_FILES_PATH}"
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

add_executable("Daemon" src/Daemon.cpp)

target_compile_definitions(Daemon PRIVATE
      LOG_FILES_PATH="${LOG_FILES_PATH}"
)

target_link_libraries(
  "Daemon"
  PUBLIC rabbitmq
         mqss::mqss
         spdlog::spdlog
         nlohmann_json::nlohmann_json
         Common
      )

# add_executable(ExampleDriver
#     src/ExampleDriver.cpp
#     src/Daemon.cpp
#     src/MQSSCompiler.cpp
# )
# add_subdirectory(tests)