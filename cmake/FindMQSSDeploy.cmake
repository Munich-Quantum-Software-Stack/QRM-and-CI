
include(FetchContent)

FetchContent_Declare(
  MQSSDeploy
  GIT_REPOSITORY https://github.com/Munich-Quantum-Software-Stack/MQSS-Integration-Deployment-Framework.git
  GIT_TAG develop # Use the latest tag
)

FetchContent_MakeAvailable(MQSSDeploy)

