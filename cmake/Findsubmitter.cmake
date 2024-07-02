include(FetchContent)

FetchContent_Declare(
    submitter
    GIT_REPOSITORY git@github.com:Munich-Quantum-Software-Stack/submitter.git
    GIT_TAG develop
)

FetchContent_MakeAvailable(submitter)

FetchContent_GetProperties(submitter)

set(SUBMITTER_INCLUDE_DIRS "${submitter_SOURCE_DIR}/include")
