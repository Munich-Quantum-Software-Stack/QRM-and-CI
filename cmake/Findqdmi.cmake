include(FetchContent)

FetchContent_Declare(
    qdmi
    GIT_REPOSITORY git@github.com:Munich-Quantum-Software-Stack/QDMI.git
    GIT_TAG testing-darwin
)

FetchContent_MakeAvailable(qdmi)
