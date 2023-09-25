#!/bin/bash

# Arrays for receiver and decompression domain
receiver_domain=(0 1)
decompression_domain=(0 1)

# Loop over each value in the list for the thread numbers
for thread_number in 2 4 8 16 32
do
    # Loop for different receiver and decompression domains
    for r_domain in "${receiver_domain[@]}"
    do
        for d_domain in "${decompression_domain[@]}"
        do
            echo ">>>Sender Test thread number $thread_number r_domain $r_domain d_domain $d_domain"
            /home/beams/MJAMIL/hpdt4ha/tests/perf_tools/compress_and_send/compress_and_send /local/tomo_00059.h5 $thread_number $thread_number 1 0 1 164.54.112.189 8080 > /home/beams/MJAMIL/hpdt4ha/tests/perf_tools/logs/comp_send_rec_decomp/sender/log_r_node_${r_domain}d_node_${d_domain}_thread_number${thread_number}
            sleep 20
            pkill -9 -f "compress_and_send"
            echo ">>>Sender Done thread number $thread_number r_domain $r_domain d_domain $d_domain"

        done
    done
done
