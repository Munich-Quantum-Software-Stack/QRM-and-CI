# Enable FetchContent module
include(FetchContent)

# Fetch spdlog using FetchContent
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.x # You can specify a version tag here
)

# Make sure spdlog is downloaded and available
FetchContent_MakeAvailable(spdlog)
