#cmake_minimum_required(VERSION 3.10)

project(MyQirTests)

find_package(GTest REQUIRED)
include(GoogleTest)

set(TEST_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/test_ibm.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_qlm.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_wmi_real.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_wmi_sim.cpp"
)

include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src)

find_library(RABBITMQ_LIBRARY rabbitmq)

if(NOT RABBITMQ_LIBRARY)
    message(FATAL_ERROR "RabbitMQ library not found.")
endif()

foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE} ${CMAKE_CURRENT_SOURCE_DIR}/../src/connection_handling.cpp)
    target_link_libraries(${TEST_NAME} ${RABBITMQ_LIBRARY} ${GTEST_LIBRARIES})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()
