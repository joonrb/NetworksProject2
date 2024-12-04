#!/bin/bash

# Go to project root directory
cd "$(dirname "$0")"

# Create test directory if it doesn't exist
mkdir -p test_cwnd
cd test_cwnd

# Create input file
dd if=/dev/urandom of=input.txt bs=1M count=10

# Make sure the executables are built
cd ..
make -C source/

# Make sure the simulator is executable
chmod +x highway

# Create necessary files for plotting
echo "0,0" > test_cwnd/throughput.txt
echo "1000" > trace.txt

# Start receiver in background
./obj/rdt_receiver 12345 output.txt &
RECEIVER_PID=$!

# Wait for receiver to start
sleep 1

# Start network simulator in background (using highway instead of cellular)
./highway 12345 12346 &
SIMULATOR_PID=$!

# Wait for simulator to start
sleep 1

# Run sender
./obj/rdt_sender 127.0.0.1 12346 input.txt

# Wait for transfer to complete
sleep 2

# Generate plot
python3 source/plot.py -d test_cwnd -n throughput.txt -tr trace.txt

# Clean up background processes
kill $RECEIVER_PID $SIMULATOR_PID 2>/dev/null

echo "Test complete. Check test_cwnd/throughput_cwnd.pdf for results"
