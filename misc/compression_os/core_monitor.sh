#!/bin/bash

for m in 0 1; do
  for c in 1 2 4 8 16 32; do
      echo "Running for memory in node ${m} , execution in OS and thread count=${c}"

    /home/beams/MJAMIL/hpdt4ha/tests/monitor_scripts/cpu_monitor.sh /home/beams/MJAMIL/hpdt4ha/tests/perf_tools/logs/compression/OS/cpu/m${m}_e_OS_CT_${c}.log &
    monitor_pid=$!

    ./compression_os /local/tomo_00059.h5 ${c} ${m} 1 >/home/beams/MJAMIL/hpdt4ha/tests/perf_tools/logs/compression/OS/throughput/m${m}_e_OS_CT_${c}.log

    kill -9 $monitor_pid
    sleep 0.5
  done
done
