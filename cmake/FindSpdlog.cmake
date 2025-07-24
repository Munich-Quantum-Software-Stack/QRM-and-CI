# Enable FetchContent module
include(FetchContent)

# Fetch spdlog using FetchContent
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.12.0 # You can specify a version tag here
)

# Make sure spdlog is downloaded and available
FetchContent_MakeAvailable(spdlog)

if(TARGET spdlog)
  install(
    TARGETS spdlog
    EXPORT spdlogConfig
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    INCLUDES
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
endif()
