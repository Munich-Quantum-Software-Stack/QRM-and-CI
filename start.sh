#!/bin/bash
# start.sh
cd build

./Daemon &
./MQSSCompiler

echo "All services started. PIDs: $!"
echo "Press Ctrl+C to stop all"

# trap Ctrl+C and kill all background jobs
trap "kill $(jobs -p); echo 'Stopped.'" SIGINT
wait