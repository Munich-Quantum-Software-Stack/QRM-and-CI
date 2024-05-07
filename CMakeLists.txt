cmake_minimum_required(VERSION 3.22...3.28)
project(
  QuantumResourceManager
  VERSION 0.1
  DESCRIPTION "Quantum Resource Manager")

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)

# Set the installation directory for the daemon
#if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
set(CMAKE_INSTALL_PREFIX $ENV{HOME}/bin CACHE PATH "..." FORCE)
#endif()
set(INSTALL_PATH "${CMAKE_INSTALL_PREFIX}")

# Create the qresourcemanager_d.log file
file(WRITE ${CMAKE_INSTALL_PREFIX}/logs/qresourcemanager_d.log "")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(cmake/ExternalDependencies.cmake)

# Set LLVM_ENABLE_THREADS to ON
set(LLVM_ENABLE_THREADS ON)

# Set up LLVM-specific flags
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# # Add the include paths
# include_directories(
#   "${CMAKE_CURRENT_SOURCE_DIR}/include"
#   "${CMAKE_CURRENT_SOURCE_DIR}/include/generator_runner"
#   "${CMAKE_CURRENT_SOURCE_DIR}/include/pass_runner"
#   "${CMAKE_CURRENT_SOURCE_DIR}/include/selector_runner"
#   "${CMAKE_CURRENT_SOURCE_DIR}/include/scheduler_runner"
#   "${CMAKE_INSTALL_PREFIX}/include"
#   ${LLVM_INCLUDE_DIRS}
#   /opt/homebrew/Cellar/nlohmann-json/3.11.3/include #TODO
#   /opt/homebrew/Cellar/rabbitmq-c/0.14.0/include #TODO
# )

# Build shared libraries
add_library(QirPassRunner   SHARED src/pass_runner/QirPassRunner.cpp)
add_library(PassRunner      SHARED src/pass_runner/PassRunner.cpp)
add_library(SelectorRunner  SHARED src/selector_runner/SelectorRunner.cpp)
add_library(SchedulerRunner SHARED src/scheduler_runner/SchedulerRunner.cpp)
add_library(GeneratorRunner SHARED src/generator_runner/GeneratorRunner.cpp)

target_include_directories(QirPassRunner   PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/pass_runner
                                                  ${LLVM_INCLUDE_DIRS})
target_include_directories(PassRunner   PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/pass_runner)
target_include_directories(SelectorRunner  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/selector_runner
                                                  ${LLVM_INCLUDE_DIRS})
target_include_directories(SchedulerRunner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/scheduler_runner
                                                  ${CMAKE_CURRENT_SOURCE_DIR}/include/pass_runner
                                                  ${CMAKE_CURRENT_SOURCE_DIR}/include/generator_runner
                                                  ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(GeneratorRunner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/generator_runner
                                                  ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Find the necessary LLVM components
llvm_map_components_to_libnames(LLVM_LIBS support
                                          core
                                          mcjit
                                          analysis
                                          scalaropts
                                          codegen
                                          transformutils
                                          ipo
                                          objcarcopts
                                          vectorize
                                          instcombine
                                          bitwriter)

target_link_libraries(QirPassRunner   PRIVATE qdmi qinfo fomac
                                              ${LLVM_LIBS})
target_link_libraries(PassRunner      PRIVATE QirPassRunner
                                              qdmi qinfo fomac
                                              ${LLVM_LIBS})
target_link_libraries(SelectorRunner  PRIVATE PassRunner
                                              qdmi qinfo fomac
                                              ${LLVM_LIBS})
target_link_libraries(SchedulerRunner PRIVATE SelectorRunner
                                              qdmi qinfo fomac
                                              nlohmann_json::nlohmann_json
                                              rabbitmq::rabbitmq
                                              ${LLVM_LIBS})
target_link_libraries(GeneratorRunner PRIVATE PassRunner SelectorRunner SchedulerRunner
                                              qdmi qinfo fomac
                                              nlohmann_json::nlohmann_json
                                              rabbitmq::rabbitmq
                                              ${LLVM_LIBS})

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
  ${LLVM_LIBS}
  rabbitmq::rabbitmq
  nlohmann_json::nlohmann_json
  Threads::Threads
  GeneratorRunner
  SchedulerRunner
  PassRunner
  SelectorRunner
  QirPassRunner
)

# Include header files
target_include_directories(${EXECUTABLE_NAME}
    PRIVATE ${CMAKE_INSTALL_PREFIX}/include
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
install(TARGETS PassRunner
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(TARGETS SelectorRunner
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
