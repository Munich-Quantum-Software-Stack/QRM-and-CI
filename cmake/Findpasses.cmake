include(FetchContent)

FetchContent_Declare(
    passes
    GIT_REPOSITORY git@github.com:Munich-Quantum-Software-Stack/passes.git
    GIT_TAG develop
)

FetchContent_MakeAvailable(passes)

FetchContent_GetProperties(passes)

set(PASSES_INCLUDE_DIRS "${passes_SOURCE_DIR}/include")
