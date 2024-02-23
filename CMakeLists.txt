cmake_minimum_required(VERSION 3.22.1)
project(
  QuantumResourceManager
  VERSION 0.1
  DESCRIPTION "Quantum Resource Manager")

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)

# Set the installation directory for the daemon
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
set(CMAKE_INSTALL_PREFIX $ENV{HOME}/bin CACHE PATH "..." FORCE)
endif()
set(INSTALL_PATH "${CMAKE_INSTALL_PREFIX}")

# Create the qresourcemanager_d.log file
file(WRITE ${CMAKE_INSTALL_PREFIX}/logs/qresourcemanager_d.log "")

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# Find FoMaC package
find_package(fomac REQUIRED)

# Find QDMI package
find_package(qdmi REQUIRED)

# Find QInfo package
find_package(qinfo REQUIRED)

# Find nlohmann_json package
find_package(nlohmann_json REQUIRED)

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
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/generator_runner"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/pass_runner"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/selector_runner"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/scheduler_runner"
  "${CMAKE_INSTALL_PREFIX}/include"
  ${LLVM_INCLUDE_DIRS})

# Build shared libraries
add_library(GeneratorRunner SHARED src/generator_runner/GeneratorRunner.cpp)
add_library(SchedulerRunner SHARED src/scheduler_runner/SchedulerRunner.cpp)
add_library(SelectorRunner  SHARED src/selector_runner/SelectorRunner.cpp)
add_library(PassRunner      SHARED src/pass_runner/PassRunner.cpp)
add_library(QirPassRunner   SHARED src/pass_runner/QirPassRunner.cpp)

target_link_libraries(GeneratorRunner PRIVATE qdmi qinfo fomac)
target_link_libraries(SchedulerRunner PRIVATE qdmi qinfo fomac)
target_link_libraries(SelectorRunner  PRIVATE qdmi qinfo fomac)
target_link_libraries(PassRunner      PRIVATE qdmi qinfo fomac)
target_link_libraries(QirPassRunner   PRIVATE qdmi qinfo fomac)

set(EXECUTABLE_NAME
    "qresourcemanager_d"
    CACHE STRING "" FORCE)

# Build the executable
set(SOURCES src/qresourcemanager_d.cpp src/connection_handling.cpp)
set(PUBLIC_QRM_HEADERS
    "include/connection_handling.hpp"
    "include/QuantumResourceManager.hpp"
    "include/pass_runner/PassModule.hpp"
    "include/pass_runner/PassRunner.hpp"
    "include/pass_runner/QirPassRunner.hpp"
    "include/pass_runner/llvm.hpp"
    "include/scheduler_runner/SchedulerRunner.hpp"
    "include/selector_runner/SelectorRunner.hpp"
    "include/generator_runner/GeneratorRunner.hpp"
)

# Build the executable
add_executable(${EXECUTABLE_NAME} ${SOURCES})

set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_INSTALL_PREFIX}
    PUBLIC_HEADER "${PUBLIC_QRM_HEADERS}"
    INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

# Include libraries subdirectories
add_subdirectory(src/generator_runner/generators)
add_subdirectory(src/selector_runner/selectors)
add_subdirectory(src/scheduler_runner/schedulers)

# Link the executable with the required libraries
target_link_libraries(${EXECUTABLE_NAME} PRIVATE
  qdmi
  qinfo
  fomac
  LLVM
  rabbitmq
  nlohmann_json::nlohmann_json
  Threads::Threads
  GeneratorRunner
  SchedulerRunner
  SelectorRunner
  PassRunner
  QirPassRunner
)

# Include header files
target_include_directories(${EXECUTABLE_NAME}
    PRIVATE /usr/local/include #RabbitMQ
            ${CMAKE_INSTALL_PREFIX}/include
            ${CMAKE_BINARY_DIR}/_deps/qdmi-src/src #TODO
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>
)

# Install the Quantum Resource Manager
install(TARGETS ${EXECUTABLE_NAME}
    EXPORT qrmConfig
	DESTINATION ${CMAKE_INSTALL_PREFIX}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include
)
install(TARGETS QirPassRunner
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(TARGETS GeneratorRunner
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(TARGETS SchedulerRunner
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(TARGETS SelectorRunner
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(TARGETS PassRunner
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

install(
    EXPORT qrmConfig
    FILE qrmConfig.cmake
    DESTINATION "${CMAKE_INSTALL_PREFIX}/cmake"
    NAMESPACE qrm::
)

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
