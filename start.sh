#!/bin/bash
# start.sh
cd build

./mock-backend &
./submitter &
./scheduler &
./target-agnostic-pass-runner & 
./target-specific-pass-runner &
./hpc-offload-listener &
./tests/test-hpc-qrm-simple

echo "All services started. PIDs: $!"
echo "Press Ctrl+C to stop all"

# trap Ctrl+C and kill all background jobs
trap "kill $(jobs -p); echo 'Stopped.'" SIGINT
wait