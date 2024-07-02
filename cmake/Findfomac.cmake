include(FetchContent)

FetchContent_Declare(
    fomac
    GIT_REPOSITORY git@github.com:Munich-Quantum-Software-Stack/FoMaC.git
    GIT_TAG develop
)

FetchContent_MakeAvailable(fomac)

FetchContent_GetProperties(fomac)

set(FOMAC_INCLUDE_DIRS "${fomac_SOURCE_DIR}/include")
