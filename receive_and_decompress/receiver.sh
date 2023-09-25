#!/bin/bash

# Check if exactly two arguments were provided
if [ $# -ne 4 ]; then
    echo "Error: Exactly two arguments are required."
    exit 1
fi
decompression_thread_num=$1
receiver_thread_num=$2
compression_thread_num=$3
receiver_numa_domain=$4
echo "The first argument for receiver is: $decompression_thread_num"
echo "The second argument for receiver is: $receiver_thread_num"
echo "The third argument for receiver is: $compression_thread_num"
echo "The fourth argument for receiver is: $receiver_numa_domain"
# /home/beams/MJAMIL/hpdt4ha/tests/monitor_scripts/cpu_monitor.sh /home/beams/MJAMIL/hpdt4ha/tests/perf_tools/logs/transfer/send_1_receive_1/receiver_core/e_${numa_domain}_CT_${thread_num}.log &
# monitor_pid=$!
DIR="/home/beams/MJAMIL/hpdt4ha/tests/perf_tools/logs/End_to_End/compression_${compression_thread_num}_decompression_${decompression_thread_num}_send_receive_${receiver_thread_num}_receiver_${receiver_numa_domain}/"
if [ ! -d "$DIR" ]; then
    # create the directory
    mkdir -p "$DIR"
    echo "Directory '$DIR' created."
else
    echo "Directory '$DIR' already exists."
fi

/home/beams/MJAMIL/hpdt4ha/tests/perf_tools/receive_and_decompress/receive_and_decompress ${receiver_thread_num} ${decompression_thread_num} ${receiver_numa_domain} 0 tcp://164.54.113.195:8080 >>"${DIR}_8080.log" &
/home/beams/MJAMIL/hpdt4ha/tests/perf_tools/receive_and_decompress/receive_and_decompress ${receiver_thread_num} ${decompression_thread_num} ${receiver_numa_domain} 0 tcp://164.54.113.195:7080 >>"${DIR}_7080.log"&

# /home/beams/MJAMIL/hpdt4ha/tests/perf_tools/multi-threaded-push-pull/multi-puller tcp://164.54.112.189:8080 $thread_num $receiver_numa_domain >> "${DIR}e_${receiver_numa_domain}_CT_${thread_num}.log"
# /home/beams/MJAMIL/hpdt4ha/tests/perf_tools/multi-threaded-push-pull/multi-puller tcp://164.54.112.189:8080 $thread_num 2 >> "${DIR}e_${receiver_numa_domain}_CT_${thread_num}.log"

# kill -9 $monitor_pid
