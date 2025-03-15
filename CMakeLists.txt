cmake_minimum_required(VERSION 3.22.1)
project(
  QuantumResourceManager
  VERSION 0.1
  DESCRIPTION "Quantum Resource Manager")

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)

# Set the installation directory for the daemon
# if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
set(CMAKE_INSTALL_PREFIX
    $ENV{HOME}/bin
    CACHE PATH "..." FORCE)
# endif()
set(INSTALL_PATH "${CMAKE_INSTALL_PREFIX}")

# Create the qresourcemanager_d.log file
file(WRITE ${CMAKE_INSTALL_PREFIX}/logs/QuantumResourceManarger.log "")

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# Find FOMAC package find_package(fomac REQUIRED) Find QDMI package
# find_package(qdmi REQUIRED) Find QInfo package find_package(qinfo REQUIRED)
# Find nlohmann_json package
find_package(nlohmann_json REQUIRED)
# Find the Threads package
find_package(Threads REQUIRED)
find_package(rabbitmq-c REQUIRED)

# Set LLVM_ENABLE_THREADS to ON set(LLVM_ENABLE_THREADS ON)

# Find LLVMConfig.cmake
find_path(LLVM_CONFIG_DIR LLVMConfig.cmake HINTS "${CMAKE_PREFIX_PATH}" ENV
                                                 LLVM_DIR)
# Add the include paths
include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/mqss"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/mqss/Utils"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/mqss/QuantumResourceManager")

add_subdirectory(src)

# Build shared libraries add_library(GeneratorRunner SHARED
# src/generator_runner/GeneratorRunner.cpp) add_library(SchedulerRunner SHARED
# src/scheduler_runner/SchedulerRunner.cpp) add_library(SelectorRunner  SHARED
# src/selector_runner/SelectorRunner.cpp) add_library(PassRunner      SHARED
# src/pass_runner/PassRunner.cpp) add_library(QirPassRunner   SHARED
# src/pass_runner/QirPassRunner.cpp)

# target_link_libraries(GeneratorRunner PRIVATE qdmi qinfo fomac)
# target_link_libraries(SchedulerRunner PRIVATE qdmi qinfo fomac)
# target_link_libraries(SelectorRunner  PRIVATE qdmi qinfo fomac)
# target_link_libraries(PassRunner      PRIVATE qdmi qinfo fomac)
# target_link_libraries(QirPassRunner   PRIVATE qdmi qinfo fomac)

set(EXECUTABLE_NAME
    "qresourcemanager_d"
    CACHE STRING "" FORCE)

# Build the executable
set(SOURCES src/QuantumResourceManager.cpp src/ConnectionHandler.cpp)
set(PUBLIC_QRM_HEADERS "include/mqss/QuantumResourceManager.hpp"
                       "include/mqss/ConnectionHandler.hpp")

# Build the executable
add_executable(${EXECUTABLE_NAME} ${SOURCES})

set_target_properties(
  ${EXECUTABLE_NAME}
  PROPERTIES VERSION ${PROJECT_VERSION}
             SOVERSION ${PROJECT_VERSION_MAJOR}
             RUNTIME_OUTPUT_DIRECTORY ${CMAKE_INSTALL_PREFIX}
             PUBLIC_HEADER "${PUBLIC_QRM_HEADERS}"
             INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib"
             BUILD_WITH_INSTALL_RPATH TRUE)

# Include libraries subdirectories
# add_subdirectory(src/generator_runner/generators)
# add_subdirectory(src/selector_runner/selectors)
# add_subdirectory(src/scheduler_runner/schedulers)

# Link the executable with the required libraries
target_link_libraries(
  ${EXECUTABLE_NAME}
  PRIVATE rabbitmq nlohmann_json::nlohmann_json Threads::Threads mqss-Utils
          spdlog::spdlog_header_only)

# Include header files
target_include_directories(
  ${EXECUTABLE_NAME}
  PRIVATE /usr/local/include # RabbitMQ
          ${CMAKE_INSTALL_PREFIX}/include
          ${CMAKE_BINARY_DIR}/_deps/qdmi-src/src # TODO
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>)

# Install the Quantum Resource Manager
install(
  TARGETS ${EXECUTABLE_NAME}
  EXPORT qrmConfig
  DESTINATION ${CMAKE_INSTALL_PREFIX}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include)

install(
  EXPORT qrmConfig
  FILE qrmConfig.cmake
  DESTINATION "${CMAKE_INSTALL_PREFIX}/cmake"
  NAMESPACE qrm::)

# Uninstall the Quantum Resource Manager
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/uninstall.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake" IMMEDIATE @ONLY)

add_custom_target(
  uninstall
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
# Setup tests
enable_testing()
add_subdirectory(tests)
