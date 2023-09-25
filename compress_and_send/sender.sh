#!/bin/bash

# Check if exactly two arguments were provided
if [ $# -ne 3 ]; then
    echo "Error: Exactly two arguments are required."
    exit 1
fi
compression_thread_num=$1
sender_thread_num=$2
sender_port=$3


echo "The first argument for sender is: $compression_thread_num"
echo "The second argument for sender is: $sender_thread_num"
echo "The third argument for sender is: $sender_port"
# /home/beams/MJAMIL/hpdt4ha/tests/monitor_scripts/cpu_monitor.sh /home/beams/MJAMIL/hpdt4ha/tests/perf_tools/logs/transfer/send_1_receive_1/sender_core/e_${numa_domain}_CT_${thread_num}.log &
# monitor_pid=$!
# /home/beams/MJAMIL/hpdt4ha/tests/perf_tools/multi-threaded-push-pull/multi-pusher tcp://164.54.112.189:8080 5351349 1500 $thread_num $sender_numa_domain
/home/beams/MJAMIL/hpdt4ha/tests/perf_tools/compress_and_send/compress_and_send /local/tomo_test.h5 $compression_thread_num $sender_thread_num 0 0 1 164.54.113.195 $sender_port 1 1
# /home/beams/MJAMIL/hpdt4ha/tests/perf_tools/multi-threaded-push-pull/multi-pusher tcp://164.54.112.189:8080 5351349 1500 $thread_num 2

# kill -9 $monitor_pid
