cmake_minimum_required(VERSION 3.22.1)
project(
  QuantumResourceManager
  VERSION 1.0
  DESCRIPTION "Quantum Resource Manager")

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)

# Set the installation directory for the daemon
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX
      $ENV{HOME}
      CACHE PATH "Installation directory" FORCE)
endif()
set(INSTALL_PATH "${CMAKE_INSTALL_PREFIX}")

# Create the qresourcemanager_d.log file
file(WRITE ${CMAKE_INSTALL_PREFIX}/logs/qresourcemanager_d.log "")

# Find nlohmann_json package
find_package(nlohmann_json 3.2.0 REQUIRED)

# Find the Threads package
find_package(Threads REQUIRED)

# Set LLVM_ENABLE_THREADS to ON
set(LLVM_ENABLE_THREADS ON)

# Find LLVMConfig.cmake
find_path(LLVM_CONFIG_DIR LLVMConfig.cmake HINTS "${CMAKE_PREFIX_PATH}" ENV
                                                 LLVM_DIR)

if(NOT LLVM_CONFIG_DIR)
  message(
    FATAL_ERROR "LLVMConfig.cmake not found. Set CMAKE_PREFIX_PATH or LLVM_DIR."
  )
endif()

# Include LLVMConfig.cmake
list(APPEND CMAKE_PREFIX_PATH "${LLVM_CONFIG_DIR}")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${LLVM_CONFIG_DIR}")
find_package(LLVM REQUIRED CONFIG)

# Set up LLVM-specific flags
add_definitions(${LLVM_DEFINITIONS})

# Add the include paths
include_directories(
  src/generator_runner
  src/pass_runner
  src/selector_runner
  src/scheduler_runner
  ${CUSTOM_QDMI_INCLUDE_PATH}
  ${CUSTOM_FOMAC_INCLUDE_PATH}
  ${CUSTOM_BACKENDS_INCLUDE_PATH}
  ${LLVM_INCLUDE_DIRS})

# Build shared libraries
add_library(GeneratorRunner SHARED src/generator_runner/GeneratorRunner.cpp)
add_library(SchedulerRunner SHARED src/scheduler_runner/SchedulerRunner.cpp)
add_library(SelectorRunner SHARED src/selector_runner/SelectorRunner.cpp)
add_library(PassRunner SHARED src/pass_runner/PassRunner.cpp)
add_library(QirPassRunner SHARED src/pass_runner/QirPassRunner.cpp)

set(EXECUTABLE_NAME
    "qresourcemanager_d"
    CACHE STRING "" FORCE)

# Build the executable
set(SOURCES src/qresourcemanager_d.cpp src/connection_handling.cpp)

# Build the executable
add_executable(${EXECUTABLE_NAME} ${SOURCES})

# Set the output directory for the executable during build
set_target_properties(${EXECUTABLE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                                    ${CMAKE_INSTALL_PREFIX}/bin)

# Set the INSTALL_RPATH for the executable
set(INSTALL_PATHS
    "${CMAKE_INSTALL_PREFIX}/bin/lib/generator_runner;"
    "${CMAKE_INSTALL_PREFIX}/bin/lib/generator_runner/generators;"
    "${CMAKE_INSTALL_PREFIX}/bin/lib/selector_runner;"
    "${CMAKE_INSTALL_PREFIX}/bin/lib/selector_runner/selectors;"
    "${CMAKE_INSTALL_PREFIX}/bin/lib/scheduler_runner;"
    "${CMAKE_INSTALL_PREFIX}/bin/lib/scheduler_runner/schedulers;"
    "${CMAKE_INSTALL_PREFIX}/bin/lib/pass_runner;"
    "${CUSTOM_QDMI_LIBRARY_PATH};"
    "${CUSTOM_BACKENDS_LIBRARY_PATH};"
    "${CUSTOM_FOMAC_LIBRARY_PATH}")
string(REPLACE ";" "\\;" RPATHS "${INSTALL_PATHS}")
set_target_properties(
  ${EXECUTABLE_NAME} PROPERTIES INSTALL_RPATH ${RPATHS} BUILD_WITH_INSTALL_RPATH
                                                        TRUE)

# Include libraries subdirectories
add_subdirectory(src/generator_runner/generators)
add_subdirectory(src/selector_runner/selectors)
add_subdirectory(src/scheduler_runner/schedulers)

# Link the executable with LLVM and Threads libraries
target_link_libraries(${EXECUTABLE_NAME} PRIVATE
  ${CUSTOM_QDMI_LIBRARY_PATH}/libQDMI.so
  LLVM
  rabbitmq
  nlohmann_json::nlohmann_json
  Threads::Threads
  GeneratorRunner
  SchedulerRunner
  SelectorRunner
  PassRunner
  QirPassRunner)

# Include RabbitMQ, QDMI, FoMac, and the Backends header files
target_include_directories(
  ${EXECUTABLE_NAME} PRIVATE /usr/local/include)

# Install the Quantum Resource Manager
install(TARGETS ${EXECUTABLE_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS QirPassRunner
        DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/lib/pass_runner)
install(TARGETS GeneratorRunner
        DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/lib/generator_runner)
install(TARGETS SchedulerRunner
        DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/lib/scheduler_runner)
install(TARGETS SelectorRunner
        DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/lib/selector_runner)
install(TARGETS PassRunner
        DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/lib/pass_runner)

# Uninstall the Quantum Resource Manager
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/uninstall.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake" IMMEDIATE @ONLY)

add_custom_target(
  uninstall
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Build the documentation
option(BUILD_WITH_DOCS "Generate Docs" OFF)
if(BUILD_WITH_DOCS)
  set(BUILD_DOCS_STANDALONE OFF)
  message("Building documentation.")
  set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/docs)
endif()

# Setup tests
enable_testing()
add_subdirectory(tests)
