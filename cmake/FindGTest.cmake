# cmake/FetchGTest.cmake
#
# Fetches and configures GoogleTest via FetchContent.
# Include this from your top-level CMakeLists.txt with:
#   include(cmake/FetchGTest.cmake)
#
# Provides the standard targets: GTest::gtest, GTest::gtest_main,
# GTest::gmock, GTest::gmock_main

include(FetchContent)

# Pin to a specific release for reproducible builds.
set(GTEST_VERSION "1.15.2" CACHE STRING "GoogleTest version to fetch")

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v${GTEST_VERSION}
  GIT_SHALLOW    TRUE
)

# Required on Windows; harmless elsewhere. Keeps GTest's runtime library
# selection in sync with the rest of the project instead of forcing its own.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Avoid installing GTest alongside your own project's install target.
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# Silence warnings from GTest's own sources when building with -Wall -Wextra
# on the rest of the project.
if(TARGET gtest)
  target_compile_options(gtest PRIVATE -w)
endif()
if(TARGET gtest_main)
  target_compile_options(gtest_main PRIVATE -w)
endif()
if(TARGET gmock)
  target_compile_options(gmock PRIVATE -w)
endif()
if(TARGET gmock_main)
  target_compile_options(gmock_main PRIVATE -w)
endif()