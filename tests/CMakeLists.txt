project(MyQirTests)

# Configure the Google Test framework.
set(TEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/test.cpp")

include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

add_executable(QirTest ${TEST_SOURCE} ../src/connection_handling.cpp)

target_link_libraries(QirTest
    rabbitmq::rabbitmq
    nlohmann_json::nlohmann_json
    gtest_main)

# Automatically discover and run tests using CTest.
include(GoogleTest)
add_test(NAME QirTest COMMAND QirTest)
