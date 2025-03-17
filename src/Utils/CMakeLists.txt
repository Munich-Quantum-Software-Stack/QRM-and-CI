find_package(Spdlog REQUIRED)

add_library(Utils STATIC Logger.cpp)
# Link spdlog to the logger library
target_link_libraries(Utils PRIVATE spdlog::spdlog)
