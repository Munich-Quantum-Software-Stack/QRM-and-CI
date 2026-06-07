include(FetchContent)

set(BUILD_MODULES "lrz" CACHE STRING "" FORCE)
FetchContent_Declare(
  mqss-devices
  GIT_REPOSITORY https://github.com/Munich-Quantum-Software-Stack/MQSS-QDMI-Devices-Suite.git
  GIT_TAG develop)

FetchContent_MakeAvailable(mqss-devices)
