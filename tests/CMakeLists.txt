project(MyQirTests)


add_compile_options("-g")

# Locate and configure the Google Test framework.
find_package(GTest REQUIRED)

find_library(ONNXRUNTIME_LIBRARY NAMES onnxruntime PATHS /home/ubuntu/onnxruntime/build/Linux/Release)

set(TEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/test.cpp")
set(TEST_SOURCE2 "${CMAKE_CURRENT_SOURCE_DIR}/QRMTest.cpp")

include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src)

#find_library(RABBITMQ_LIBRARY rabbitmq)

add_executable(QirTest ${TEST_SOURCE} ../src/connection_handling.cpp)
add_executable(QRMTest ${TEST_SOURCE2} ../src/connection_handling.cpp ../src/pass_runner/QirPassRunner.cpp ../src/predictor.cpp ../src/eval.cpp)

#target_link_libraries(QirTest ${RABBITMQ_LIBRARY} ${GTEST_LIBRARIES})
target_link_libraries(QirTest rabbitmq)
target_link_libraries(QRMTest rabbitmq LLVM QRMD QirPassRunner ${ONNXRUNTIME_LIBRARY})

# Automatically discover and run tests using CTest.
include(GoogleTest)
add_test(NAME QirTest COMMAND QirTest)
add_test(NAME QRMTest COMMAND QRMTest)
