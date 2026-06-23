#!/bin/bash

./build/MQSSScheduler &
SCHEDULER_PID=$!

./build/MQSSSubmitter &
SUBMITTER_PID=$!

./build/MQSSCompiler &
COMPILER_PID=$!

./build/tests/test-hpc-qrm-simple &
TEST_PID=$!

echo "All services started."
echo "  Scheduler PID: $SUBMITTER_PID"
echo "  Scheduler PID: $SCHEDULER_PID"
echo "  Compiler  PID: $COMPILER_PID"
echo "  Test      PID: $TEST_PID"
echo "Press Ctrl+C to stop all"

# Quote the variable expansion to defer evaluation to signal time
trap "kill -SIGTERM $SUBMITTER_PID $SCHEDULER_PID $COMPILER_PID $TEST_PID; wait; echo 'Stopped.'" SIGINT SIGTERM

wait