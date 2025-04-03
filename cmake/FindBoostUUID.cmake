include(FetchContent)

FetchContent_Declare(
  BoostUUID
  GIT_REPOSITORY https://github.com/boostorg/uuid.git
  GIT_TAG boost-1.74.0)
FetchContent_MakeAvailable(BoostUUID)
