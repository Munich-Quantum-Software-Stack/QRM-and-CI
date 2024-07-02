cmake_minimum_required(VERSION 3.22.1)
project(
  QuantumResourceManager
  VERSION 0.1
  DESCRIPTION "Quantum Resource Manager")

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options("-g")

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})


# Find QDMI package
find_package(qdmi REQUIRED)

# Find FoMaC package
find_package(fomac REQUIRED)

# Find QInfo package
find_package(qinfo REQUIRED)

# Find Selector package
find_package(selector REQUIRED)

# Find Passes package
find_package(passes REQUIRED)

# Find nlohmann_json package
find_package(nlohmann_json REQUIRED)

# Find the Threads package
find_package(Threads REQUIRED)

# Find the Generator package
find_package(generator REQUIRED)

# Find the Submitter package
find_package(submitter REQUIRED)

# Find the Threads package
find_package(scheduler REQUIRED)

# Find the LLVM package
find_package(LLVM REQUIRED CONFIG)

# Set LLVM_ENABLE_THREADS to ON
set(LLVM_ENABLE_THREADS ON)


# Set up LLVM-specific flags
add_definitions(${LLVM_DEFINITIONS})

# Add the include paths
include_directories(
  ${GENERATOR_INCLUDE_DIRS}
  ${PASSES_INCLUDE_DIRS}
  ${QINFO_INCLUDE_DIRS}
  ${QDMI_INCLUDE_DIRS}
  ${FOMAC_INCLUDE_DIRS}
  ${SCHEDULER_INCLUDE_DIRS}
  ${SUBMITTER_INCLUDE_DIRS}
  include
  ${LLVM_INCLUDE_DIRS}
)

# Build shared libraries

set(EXECUTABLE_NAME
    "qresourcemanager_d"
    CACHE STRING "" FORCE)

# Build the executable
set(SOURCES src/*.cpp
            )

# Build the executable
add_executable(${EXECUTABLE_NAME} ${SOURCES})

# Link the executable with the required libraries
target_link_libraries(${EXECUTABLE_NAME} PRIVATE
  qdmi
  qinfo
  fomac
  submitter
  selector
  scheduler
  generator
  LLVM
  rabbitmq 
  nlohmann_json::nlohmann_json
  Threads::Threads
)

add_subdirectory(tests)
