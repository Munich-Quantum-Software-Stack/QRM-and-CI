# Enable FetchContent module
include(FetchContent)

# Fetch spdlog using FetchContent
FetchContent_Declare(
  rabbitmq-c
  GIT_REPOSITORY https://github.com/alanxz/rabbitmq-c.git
  GIT_TAG v0.13.0)

# Make sure spdlog is downloaded and available
FetchContent_MakeAvailable(rabbitmq-c)

if(TARGET rabbitmq)
  install(
    TARGETS rabbitmq
    EXPORT rabbitmqConfig
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    INCLUDES
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
endif()
