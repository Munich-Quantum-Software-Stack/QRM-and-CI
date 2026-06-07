find_package(Spdlog REQUIRED)

add_library(Common STATIC Logger.cpp QuantumTaskConverter.cpp
                          RabbitMQClient.cpp RabbitMQServer.cpp TaskStatus.cpp)
# Link spdlog to the logger library
target_link_libraries(
  Common
  PRIVATE spdlog::spdlog
          rabbitmq
          uuid
          qdmi::qdmi
          lrz_device 
          qdmi::project_warnings
          MLIRIR
          MLIRPass
          MLIRTransforms
          ${CUDAQ_TARGETS})
