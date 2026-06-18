include(FetchContent)

FetchContent_Declare(
  Submitter
  GIT_REPOSITORY https://github.com/Munich-Quantum-Software-Stack/MQSS-Submitter.git
  GIT_TAG 77e119ab77cafdfb1ea6d888bf91dae1c7e0258e # Use the latest tag
)

FetchContent_MakeAvailable(Submitter)