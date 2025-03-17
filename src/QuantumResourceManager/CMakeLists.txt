add_library(PassRunner STATIC PassRunner.cpp)
# Link spdlog to the logger library
target_link_libraries(PassRunner PRIVATE MQSS::Optimizer)
