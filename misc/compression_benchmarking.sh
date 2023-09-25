#!/bin/bash

# Array of thread numbers
thread_numbers=(1 2 4 8 16 32 64 128)

for num in "${thread_numbers[@]}"
do
    ./reader /home/beams/MJAMIL/streamFile/tomo_00059.h5 $num > ./logs/OS/output_${num}.txt
done
