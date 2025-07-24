cmake_minimum_required(VERSION 3.22.1)
project(
  QuantumResourceManager
  VERSION 0.1
  DESCRIPTION "Quantum Resource Manager"
  LANGUAGES C CXX)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
set(CMAKE_CXX_FLAGS "-w -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-switch")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frtti -fexceptions")

# Set default build type to Debug if none is specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      Debug
      CACHE STRING "Build type (Debug, Release)" FORCE)
endif()

# Define macros for Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  message("Configuring for Debug mode")
  add_compile_definitions(DEBUG)
endif()

# Set the installation directory for the daemon
# if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
set(CMAKE_INSTALL_PREFIX
    $ENV{HOME}/bin
    CACHE PATH "..." FORCE)
# endif()
set(INSTALL_PATH "${CMAKE_INSTALL_PREFIX}")

# Create the qresourcemanager_d.log file
file(WRITE ${CMAKE_INSTALL_PREFIX}/logs/QuantumResourceManarger.log "")
# Tell where to find fetch content, etc.
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

find_package(MLIR REQUIRED CONFIG)
message(STATUS "Using MLIRConfig.cmake in: ${MLIR_DIR}")

list(APPEND CMAKE_MODULE_PATH "${MLIR_CMAKE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules")

include(TableGen)
include(AddLLVM)
include(AddMLIR)

# Add LLVM, Clang and MLIR headers to the include path
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS}
                    ${MLIR_INCLUDE_DIRS})
# ##############################################################################
# Find FOMAC package find_package(fomac REQUIRED) Find QDMI package
find_package(qdmi REQUIRED)
# Find QInfo package find_package(qinfo REQUIRED) Find nlohmann_json package
find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)
# Find the Threads package
find_package(Threads REQUIRED)
find_package(rabbitmq-c REQUIRED)
find_package(MLIRPasses REQUIRED)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost REQUIRED)
message(STATUS "Boost version: ${Boost_VERSION}")
if(NOT Boost_FOUND)
  message(FATAL_ERROR "Boost library not found.")
endif()
find_package(BoostUUID REQUIRED)

# configure the MLIR Passes repo
set(PASSES_SOURCE_DIR ${MLIRPasses_SOURCE_DIR})
set(PASSES_INCLUDE_DIR "${PASSES_SOURCE_DIR}/include")
set(PASSES_TB_INCLUDE_DIR "${MLIRPasses_BINARY_DIR}/include")
include_directories(${PASSES_INCLUDE_DIR})
include_directories(${PASSES_TB_INCLUDE_DIR})

set(MQT_QMAP_INCLUDE_DIR "${mqt-qmap_SOURCE_DIR}/include/")
set(MQT_QCEC_INCLUDE_DIR "${mqt-qcec_SOURCE_DIR}/include/")
set(MQT_CORE_INCLUDE_DIR "${mqt-core_SOURCE_DIR}/include/mqt-core")

# Add the include paths
include_directories(
  BEFORE
  ${LLVM_INCLUDE_DIRS}
  ${MLIR_INCLUDE_DIRS}
  ${MQT_QMAP_INCLUDE_DIR}
  ${MQT_CORE_INCLUDE_DIR}
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/mqss"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/mqss/Utils"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/mqss/QuantumResourceManager"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/mqss/QuantumResourceManager/PassManager")

# ###################### CUDAQ settings ###################################
set(CUDAQ_MAIN_INCLUDE_DIR "${CUDAQ_SOURCE_DIR}/include")

set(CUDAQ_BINARY_DIR "${CUDAQ_SOURCE_DIR}/build")
set(CUDAQ_CMAKE_DIR "${CUDAQ_SOURCE_DIR}/cmake")
set(CUDAQ_INCLUDE_DIR "${CUDAQ_SOURCE_DIR}/build/include")
set(CUDAQ_LIBRARY_DIR "${CUDAQ_SOURCE_DIR}/build/lib")
set(CUDAQ_TOOLS_DIR "${CUDAQ_SOURCE_DIR}/build/bin")
set(CUDAQ_MLIR_RUNTIME_PATH "${CUDAQ_BINARY_DIR}/lib")
message(STATUS "CUDAQ bin ${CUDAQ_MLIR_RUNTIME_PATH}")

include_directories(BEFORE ${CUDAQ_MAIN_INCLUDE_DIR}
                    ${CUDAQ_BINARY_DIR}/include ${CUDAQ_SOURCE_DIR}/runtime)

# Validate CUDAQ dependencies
if(NOT EXISTS ${CUDAQ_BINARY_DIR}/lib/libcudaq-mlir-runtime.so)
  message(
    FATAL_ERROR
      "Invalid CUDAQ dir: ${CUDAQ_SOURCE_DIR}. Cannot find libcudaq.so in build directory."
  )
endif()
set(CUDAQ_TARGETS ${CUDAQ_MLIR_RUNTIME_PATH}/libcudaq-mlir-runtime.so)
# ##############################################################################
add_subdirectory(src)

set(PUBLIC_QRM_HEADERS "include/mqss/QuantumResourceManager.hpp"
                       "include/mqss/ConnectionHandler.hpp")
# Build the executable
add_executable("hpc-offload-listener" src/HPC-Offload-Listener.cpp)
# Link the executable with the required libraries
target_link_libraries(
  "hpc-offload-listener" PUBLIC rabbitmq nlohmann_json::nlohmann_json
                                spdlog::spdlog Threads::Threads Common)

# Build the executable
add_executable("mqp-offload-listener" src/MQP-Offload-Listener.cpp)
# Link the executable with the required libraries
target_link_libraries(
  "mqp-offload-listener" PUBLIC rabbitmq nlohmann_json::nlohmann_json
                                spdlog::spdlog Threads::Threads Common)

add_executable("agnostic-pass-runner" src/AgnosticPassRunner.cpp)
target_link_libraries(
  "agnostic-pass-runner"
  PUBLIC rabbitmq
         spdlog::spdlog
         nlohmann_json::nlohmann_json
         Threads::Threads
         Common
         MLIRIR
         MLIRPass
         MLIRTransforms
         ${CUDAQ_MLIR_RUNTIME_PATH}/libcudaq-mlir-runtime.so
         PassRunner
         fmt::fmt)

add_executable("scheduler" src/Scheduler.cpp)
target_link_libraries("scheduler" PUBLIC rabbitmq nlohmann_json::nlohmann_json
                                         Threads::Threads Common spdlog::spdlog)

add_executable("target-specific-pass-runner" src/TargetSpecificPassRunner.cpp)
target_link_libraries(
  "target-specific-pass-runner"
  PUBLIC rabbitmq
         spdlog::spdlog
         nlohmann_json::nlohmann_json
         Threads::Threads
         Common
         MLIRIR
         MLIRPass
         MLIRTransforms
         ${CUDAQ_MLIR_RUNTIME_PATH}/libcudaq-mlir-runtime.so
         PassRunner
         fmt::fmt)

add_executable("submitter" src/Submitter.cpp)
target_link_libraries("submitter" PUBLIC rabbitmq nlohmann_json::nlohmann_json
                                         Threads::Threads Common spdlog::spdlog)

add_executable("mock-backend" src/MockBackend.cpp)

target_include_directories(mock-backend PRIVATE ${MLIR_INCLUDE_DIRS}
                                                ${LLVM_INCLUDE_DIRS})

target_compile_definitions(mock-backend PRIVATE ${MLIR_DEFINITIONS}
                                                ${LLVM_DEFINITIONS})

target_link_libraries(
  "mock-backend"
  PUBLIC rabbitmq
         spdlog::spdlog
         nlohmann_json::nlohmann_json
         Threads::Threads
         Common
         fmt::fmt
         # Required MLIR components
         MLIRIR
         MLIRPass
         MLIRTransforms
         MLIROptLib
         MLIRExecutionEngine
         ${CUDAQ_MLIR_RUNTIME_PATH}/libcudaq-mlir-runtime.so)

enable_testing()
add_subdirectory(tests)
# Enable FetchContent for integration
include(FetchContent)
