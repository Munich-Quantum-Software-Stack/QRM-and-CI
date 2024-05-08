# Declare all external dependencies and make sure that they are available.

include(FetchContent)
set(FETCH_PACKAGES "")

# Find jansson package
set(JANSSON_VERSION
    2.14
    CACHE STRING "jansson version")
set(JANSSON_URL https://github.com/akheron/jansson/releases/download/v${JANSSON_VERSION}/jansson-${JANSSON_VERSION}.tar.gz)
set(JANSSON_BUILD_DOCS OFF CACHE BOOL "" FORCE)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
  FetchContent_Declare(jansson URL ${JANSSON_URL} FIND_PACKAGE_ARGS ${JANSSON_VERSION})
  list(APPEND FETCH_PACKAGES jansson)
else()
  find_package(jansson ${JANSSON_VERSION} QUIET)
  if(NOT jansson_FOUND)
    FetchContent_Declare(jansson URL ${JANSSON_URL})
    list(APPEND FETCH_PACKAGES jansson)
  endif()
endif()

# Find rabbitmq-c package
set(RABBITMQ_C_VERSION
    0.14.0
    CACHE STRING "rabbitmq-c version")
set(RABBITMQ_C_URL https://github.com/alanxz/rabbitmq-c/archive/refs/tags/v${RABBITMQ_C_VERSION}.tar.gz)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
  FetchContent_Declare(rabbitmq-c URL ${RABBITMQ_C_URL} FIND_PACKAGE_ARGS ${RABBITMQ_C_VERSION})
  list(APPEND FETCH_PACKAGES rabbitmq-c)
else()
  find_package(rabbitmq-c ${RABBITMQ_C_VERSION} QUIET)
  if(NOT rabbitmq-c_FOUND)
    FetchContent_Declare(rabbitmq-c URL ${RABBITMQ_C_URL})
    list(APPEND FETCH_PACKAGES rabbitmq-c)
  endif()
endif()

# Find FoMaC package
set(FOMAC_SOURCE_DIR "${PROJECT_SOURCE_DIR}/../fomac")
if (EXISTS "${FOMAC_SOURCE_DIR}/CMakeLists.txt")
  set(FETCHCONTENT_SOURCE_DIR_FOMAC
      ${FOMAC_SOURCE_DIR}
      CACHE
        PATH
        "Path to the source directory of the library. This variable is used by FetchContent to download the library if it is not already available."
  )
endif()
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
  FetchContent_Declare(fomac GIT_REPOSITORY git@github.com:Munich-Quantum-Software-Stack/FoMaC.git GIT_TAG wip-cda)
  list(APPEND FETCH_PACKAGES fomac)
else()
  find_package(fomac QUIET)
  if(NOT fomac_FOUND)
    FetchContent_Declare(fomac GIT_REPOSITORY git@github.com:Munich-Quantum-Software-Stack/FoMaC.git GIT_TAG wip-cda)
    list(APPEND FETCH_PACKAGES fomac)
  endif()
endif()

# Find QDMI package
set(QDMI_SOURCE_DIR "${PROJECT_SOURCE_DIR}/../qdmi")
if (EXISTS "${QDMI_SOURCE_DIR}/CMakeLists.txt")
  set(FETCHCONTENT_SOURCE_DIR_QDMI
      ${QDMI_SOURCE_DIR}
      CACHE
        PATH
        "Path to the source directory of the library. This variable is used by FetchContent to download the library if it is not already available."
  )
endif()
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
  FetchContent_Declare(qdmi GIT_REPOSITORY git@github.com:Munich-Quantum-Software-Stack/QDMI.git GIT_TAG wip-cda)
  list(APPEND FETCH_PACKAGES qdmi)
else()
  find_package(qdmi QUIET)
  if(NOT qdmi_FOUND)
    FetchContent_Declare(qdmi GIT_REPOSITORY git@github.com:Munich-Quantum-Software-Stack/QDMI.git GIT_TAG wip-cda)
    list(APPEND FETCH_PACKAGES qdmi)
  endif()
endif()

# Find QInfo package
set(QINFO_SOURCE_DIR "${PROJECT_SOURCE_DIR}/../qinfo")
if (EXISTS "${QINFO_SOURCE_DIR}/CMakeLists.txt")
  set(FETCHCONTENT_SOURCE_DIR_QINFO
      ${QINFO_SOURCE_DIR}
      CACHE
        PATH
        "Path to the source directory of the library. This variable is used by FetchContent to download the library if it is not already available."
  )
endif()
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
  FetchContent_Declare(qinfo GIT_REPOSITORY git@github.com:Munich-Quantum-Software-Stack/QInfo.git GIT_TAG testing)
  list(APPEND FETCH_PACKAGES qinfo)
else()
  find_package(qinfo QUIET)
  if(NOT qinfo_FOUND)
    FetchContent_Declare(qinfo GIT_REPOSITORY git@github.com:Munich-Quantum-Software-Stack/QInfo.git GIT_TAG testing)
    list(APPEND FETCH_PACKAGES qinfo)
  endif()
endif()

# Find nlohmann_json package
set(JSON_VERSION
    3.11.3
    CACHE STRING "nlohmann_json version")
set(JSON_URL https://github.com/nlohmann/json/releases/download/v${JSON_VERSION}/json.tar.xz)
set(JSON_SystemInclude
    ON
    CACHE INTERNAL "Treat the library headers like system headers")
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
  FetchContent_Declare(nlohmann_json URL ${JSON_URL} FIND_PACKAGE_ARGS ${JSON_VERSION})
  list(APPEND FETCH_PACKAGES nlohmann_json)
else()
  find_package(nlohmann_json ${JSON_VERSION} QUIET)
  if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(nlohmann_json URL ${JSON_URL})
    list(APPEND FETCH_PACKAGES nlohmann_json)
  endif()
endif()

# Find the Threads package
find_package(Threads REQUIRED)

# Find LLVMConfig.cmake
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Find GTest package
set(gtest_force_shared_crt
    ON
    CACHE BOOL "" FORCE)
set(GTEST_VERSION
    1.14.0
    CACHE STRING "Google Test version")
set(GTEST_URL https://github.com/google/googletest/archive/refs/tags/v${GTEST_VERSION}.tar.gz)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
  FetchContent_Declare(googletest URL ${GTEST_URL} FIND_PACKAGE_ARGS ${GTEST_VERSION} NAMES GTest)
  list(APPEND FETCH_PACKAGES googletest)
else()
  find_package(googletest ${GTEST_VERSION} QUIET NAMES GTest)
  if(NOT googletest_FOUND)
    FetchContent_Declare(googletest URL ${GTEST_URL})
    list(APPEND FETCH_PACKAGES googletest)
  endif()
endif()

# Make all declared dependencies available.
FetchContent_MakeAvailable(${FETCH_PACKAGES})
