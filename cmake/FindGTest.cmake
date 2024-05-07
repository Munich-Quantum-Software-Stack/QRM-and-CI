include(FetchContent)

set(gtest_force_shared_crt
    ON
    CACHE BOOL "" FORCE)
set(GTEST_VERSION
    1.14.0
    CACHE STRING "Google Test version")
set(GTEST_URL https://github.com/google/googletest/archive/refs/tags/v${GTEST_VERSION}.tar.gz)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
  FetchContent_Declare(googletest URL ${GTEST_URL} FIND_PACKAGE_ARGS ${GTEST_VERSION} NAMES GTest)
  FetchContent_MakeAvailable(googletest)
else()
  find_package(googletest ${GTEST_VERSION} QUIET NAMES GTest)
  if(NOT googletest_FOUND)
    FetchContent_Declare(googletest URL ${GTEST_URL})
    FetchContent_MakeAvailable(googletest)
  endif()
endif()
