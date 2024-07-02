include(FetchContent)

FetchContent_Declare(
    generator
    GIT_REPOSITORY git@github.com:Munich-Quantum-Software-Stack/generator.git
    GIT_TAG develop
)

FetchContent_MakeAvailable(generator)

FetchContent_GetProperties(generator)

set(GENERATOR_INCLUDE_DIRS "${generator_SOURCE_DIR}/include")
