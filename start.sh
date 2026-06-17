#!/bin/bash
cd build

./MQSSScheduler &
SCHEDULER_PID=$!

./MQSSCompiler &
COMPILER_PID=$!

./tests/test-hpc-qrm-simple &
TEST_PID=$!

echo "All services started."
echo "  Scheduler PID: $SCHEDULER_PID"
echo "  Compiler  PID: $COMPILER_PID"
echo "  Test      PID: $TEST_PID"
echo "Press Ctrl+C to stop all"

# Quote the variable expansion to defer evaluation to signal time
trap "kill $SCHEDULER_PID $COMPILER_PID $TEST_PID 2>/dev/null; echo 'Stopped.'" SIGINT SIGTERM

wait