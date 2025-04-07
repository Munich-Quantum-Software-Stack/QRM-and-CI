# cmake_minimum_required(VERSION 3.10)

project(MyQirTests)

find_package(GTest REQUIRED)
include(GoogleTest)

set(TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test-qrm-simple.cpp")

include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src)

find_package(rabbitmq-c REQUIRED)

foreach(TEST_SOURCE ${TEST_SOURCES})
  get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
  add_executable(${TEST_NAME} ${TEST_SOURCE})
  target_link_libraries(${TEST_NAME} ${GTEST_LIBRARIES} rabbitmq Common
                        spdlog::spdlog_header_only)
  target_include_directories(
    ${TEST_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/mqss
                        ${CMAKE_CURRENT_SOURCE_DIR}/include/mqss/Utils)
  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()
