find_package(Spdlog REQUIRED)

add_library(Common STATIC Logger.cpp)
# Link spdlog to the logger library
target_link_libraries(
  Common
  PRIVATE spdlog::spdlog
          rabbitmq
  )
