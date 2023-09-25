#!/bin/bash

# List of reader thread counts
thread_counts=(1 2 4 8 16 32 64 128)

# Memory location is always 1
mem_loc=1

# List of thread execution locations
locations=1

# Base command
cmd="./reader_numa_balanced /home/beams/MJAMIL/streamFile/tomo_00059.h5"

# Loop over each thread count
for tc in "${thread_counts[@]}"; do
    # Loop over each thread execution location
    for exec_loc in "${locations[@]}"; do
        # Construct the command
        full_cmd="${cmd} ${tc} ${mem_loc} ${exec_loc}"
        # Run the command and save the output to a file
        ${full_cmd} > ./logs/NUMA_BALANCED/output_${mem_loc}_${exec_loc}_${tc}.txt 2>&1
    done
done
