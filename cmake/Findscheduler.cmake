include(FetchContent)

FetchContent_Declare(
    scheduler
    #GIT_REPOSITORY git@github.com:Munich-Quantum-Software-Stack/scheduler.git
    #GIT_TAG develop
    SOURCE_DIR /home/ubuntu/scheduler
)

FetchContent_MakeAvailable(scheduler)

FetchContent_GetProperties(scheduler)

set(SCHEDULER_INCLUDE_DIRS "${scheduler_SOURCE_DIR}/include")
