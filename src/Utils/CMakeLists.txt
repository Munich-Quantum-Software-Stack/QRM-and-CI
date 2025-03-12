find_package(Spdlog REQUIRED)

add_library(mqss-Utils STATIC Logger.cpp)
# Link spdlog to the logger library
target_link_libraries(mqss-Utils PRIVATE spdlog::spdlog)
