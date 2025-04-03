find_package(Spdlog REQUIRED)

add_library(Common STATIC Logger.cpp QuantumTaskConverter.cpp
                          RabbitMQClient.cpp RabbitMQServer.cpp)
# Link spdlog to the logger library
target_link_libraries(Common PRIVATE spdlog::spdlog rabbitmq uuid)
