#!/bin/bash

# List of thread counts to use in experiments
SENDER_COMPRESSION_THREAD_COUNTS=(2 4 8 16 32 )
RECEIVER_DECOMPRESSION_THREAD_COUNTS=(2 4 8 16)
SENDER_RECEIVER_THREAD_COUNTS=(2 4 8)
RECEIVER_DOMAIN=(0 1)

# sender_numa_domain=("OS")
# receiver_numa_domain=("OS")

# sender_numa_domain=(0 1)
# receiver_numa_domain=(0 1)
# Hostnames of the machines where the receiver and sender programs are installed

SENDER_HOST2="-i ~/.ssh/id_rsa_xgate updraft2"
SENDER_HOST1="-i ~/.ssh/id_rsa_xgate updraft1"
RECEIVER_HOST="-i ~/.ssh/id_rsa_xgate lynxdtn01"

# Paths to the receiver and sender programs on their respective machines
RECEIVER_PATH="/home/beams/MJAMIL/hpdt4ha/tests/perf_tools/receive_and_decompress/receiver.sh "
SENDER_PATH="/home/beams/MJAMIL/hpdt4ha/tests/perf_tools/compress_and_send/sender.sh "

RECEIVER_KILL="pkill -9 -f receive_and_decompress"
SENDER_KILL="pkill -9 -f compress_and_send"
# Run experiments
for (( i=1; i<=5; i++ )) do
    for s in "${SENDER_COMPRESSION_THREAD_COUNTS[@]}"; do
        for r in "${RECEIVER_DECOMPRESSION_THREAD_COUNTS[@]}"; do
            for n in "${SENDER_RECEIVER_THREAD_COUNTS[@]}"; do
                for m in "${RECEIVER_DOMAIN[@]}"; do
                    echo "Running experiment $i..."
                    echo "Running experiment with $s compression threads in sender $r decompression and receiver $m domain with TCP stream $n..."
                    # Start receiver in background and run next command
                    ssh $RECEIVER_HOST "$RECEIVER_PATH $r $n $s $m"  &
                    ssh $SENDER_HOST1 "$SENDER_PATH $s $n 8080" &
                    ssh $SENDER_HOST2 "$SENDER_PATH $s $n 7080" &
                    sleep 20
                    # ssh $RECEIVER_HOST "$RECEIVER_KILL" &
                    # ssh $SENDER_HOST1 "$SENDER_KILL" &
                    # ssh $SENDER_HOST2 "$SENDER_KILL" &
                done
            done
        done
    done
done
