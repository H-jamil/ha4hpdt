#!/bin/bash

for m in 0 1; do
  for n in "b"; do
    for c in 1 2 4 8 16; do
      DIR="/home/beams/MJAMIL/hpdt4ha/tests/perf_tools/logs/DE-COMPRESSION/Q_Mem_${m}_Exec_${n}/throughput/"
      if [ ! -d "$DIR" ]; then
          # create the directory
          mkdir -p "$DIR"
          echo "Directory '$DIR' created."
      else
          echo "Directory '$DIR' already exists."
      fi
      DIR_2="/home/beams/MJAMIL/hpdt4ha/tests/perf_tools/logs/DE-COMPRESSION/Q_Mem_${m}_Exec_${n}/cpu/"
      if [ ! -d "$DIR_2" ]; then
          # create the directory
          mkdir -p "$DIR_2"
          echo "Directory '$DIR_2' created."
      else
          echo "Directory '$DIR_2' already exists."
      fi
      echo "Running for Queue memory in node ${m} , decompression job execution in ${n} and thread count=${c}"
      /home/beams/MJAMIL/hpdt4ha/tests/monitor_scripts/cpu_monitor.sh  ${DIR_2}/m${m}_e_${n}_CT_${c}.log&
      monitor_pid=$!
      #"Usage: %s <hdf5_file><#compressionthreads><memory_node><compression_node><file_multiplier><queue_node><#decompression_threads><decompression_node>
      ./decompression_test /local/tomo_test.h5 32 1 1 1 ${m} ${c} 1 > ${DIR}/m${m}_e_${n}_CT_${c}.log
      kill -9 $monitor_pid
      sleep 0.5
    done
  done
done
