include(FetchContent)

FetchContent_Declare(
  MLIRPasses
  GIT_REPOSITORY https://github.com/Munich-Quantum-Software-Stack/passes.git
  GIT_TAG develop # Use the latest tag
)

FetchContent_MakeAvailable(MLIRPasses)
