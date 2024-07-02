include(FetchContent)

FetchContent_Declare(
    selector
    GIT_REPOSITORY git@github.com:Munich-Quantum-Software-Stack/selector.git
    GIT_TAG develop
)

FetchContent_MakeAvailable(selector)

FetchContent_GetProperties(selector)

set(SELECTOR_INCLUDE_DIRS "${selector_SOURCE_DIR}/include")
