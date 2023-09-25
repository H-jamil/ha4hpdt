#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"
#include "receiver.h"
#include "decompression.h"
#include "/home/beams/MJAMIL/project/libzmq/include/zmq.h"
#include <pthread.h>
#include <numa.h>
#include <numaif.h>
#include <unistd.h>
#include <errno.h>


size_t total_bytes = 0;
int total_message_count =0;
pthread_mutex_t total_bytes_lock;
size_t total_decompressed_size=0;
pthread_mutex_t total_decompressed_size_lock;
int queue_null_flag=0;
pthread_mutex_t queue_null_flag_lock;

int main(int argc, char *argv[]) {
    if (argc != 6) {           //1                  2                      3                   4                5               6                   7                   8
        printf("Usage: %s <#receiverthreads> <#decompressionthreads> <receiver_node> <decompression_node> <bind-to>\n", argv[0]);
        return 1;
    }
    int receiver_node = atoi(argv[3]);
    struct bitmask *nodemask = numa_allocate_nodemask();
    numa_bitmask_clearall(nodemask);
    numa_bitmask_setbit(nodemask, receiver_node);
    numa_bind(nodemask);
    numa_free_nodemask(nodemask);
    int num_receiver_threads = atoi(argv[1]);
    int num_decompression_threads = atoi(argv[2]);
    int decompression_node = atoi(argv[4]);
    char **bind_to;
    int message_count= 0;
    int message_size=0;
    void *watch;
    unsigned long elapsed;
    double throughput;
    double megabits;
    int rc,i;
    size_t received_bytes = 0;
    int last_number;
    char last_four_digits[5];
    Queue *queue=queue_create();
    bind_to = malloc(sizeof(char*) * num_receiver_threads);

    if (bind_to == NULL) {
        printf("Error allocating memory\n");
        return -1;
    }
    for (i = 0; i < num_receiver_threads; i++) {
        bind_to[i] = malloc(strlen(argv[5]) + 5); // "+ 5" for the integer value and null terminator
        if (bind_to[i] == NULL) {
            printf("Error allocating memory\n");
            return -1;
        }
        // Extract last four digits
        strncpy(last_four_digits, argv[5] + strlen(argv[5]) - 4, 4);
        last_four_digits[4] = '\0'; // Null terminate the string
        // Convert to integer and add i
        last_number = atoi(last_four_digits);
        last_number += i;
        // Compose new string
        strncpy(bind_to[i], argv[5], strlen(argv[5]) - 4);
        sprintf(bind_to[i] + strlen(argv[5]) - 4, "%04d", last_number);
        printf("bind_to[%d] = %s\n", i, bind_to[i]);
    }
    receive_thread_args args[num_receiver_threads-1];
    pthread_t receive_threads[num_receiver_threads-1];
    decompression_thread_args decompression_args[num_decompression_threads];
    pthread_t decompression_threads[num_decompression_threads];
    pthread_mutex_init(&total_bytes_lock, NULL);
    pthread_mutex_init(&total_decompressed_size_lock, NULL);
    pthread_mutex_init(&queue_null_flag_lock, NULL);
    for(int i = 0; i < num_decompression_threads; i++) {
        decompression_args[i].thread_id = i;
        decompression_args[i].numa_node = decompression_node;
        decompression_args[i].queue = queue;
        decompression_args[i].chunk_size = 11059200;
        pthread_create(&decompression_threads[i], NULL, dequeue_and_decompress, (void*)&decompression_args[i]);
    }
    for(int i = 1; i < num_receiver_threads; i++) {
        args[i].bind_to = bind_to[i];
        args[i].thread_id = i;
        args[i].numa_node = receiver_node;
        args[i].queue = queue;
        args[i].num_decompression_threads = num_decompression_threads;
        pthread_create(&receive_threads[i], NULL, thread_receive, (void*)&args[i]);
    }
    zmq_msg_t msg;
    void *s;
    void *ctx;
    ctx = zmq_init (1);
    if (!ctx) {
        printf ("Main thread error in zmq_init: %s\n", zmq_strerror (errno));
        return 1;
    }
    s = zmq_socket (ctx, ZMQ_PULL);
    if (!s) {
        printf ("Main thread error in zmq_socket: %s\n", zmq_strerror (errno));
        return 1;
    }

    rc = zmq_bind (s, bind_to[0]);
    if (rc != 0) {
        printf ("Main thread error in zmq_bind: %s\n", zmq_strerror (errno));
        return 1;
    }
    printf ("Main thread listening to %s\n", bind_to[0]);
    rc = zmq_msg_init (&msg);
    if (rc != 0) {
        printf ("Main thread error in zmq_msg_init: %s\n", zmq_strerror (errno));
        return 1;
    }
    rc = zmq_msg_recv(&msg, s, 0);
    if (rc < 0) {
        printf ("Main thread error in zmq_recvmsg: %s\n", zmq_strerror (errno));
        return 1;
    }
    watch = zmq_stopwatch_start ();
    received_bytes += zmq_msg_size (&msg);
    message_count++;
    rc = zmq_msg_close (&msg);
        if (rc != 0) {
          printf ("Main thread error in zmq_msg_close: %s\n", zmq_strerror (errno));
          return 1;
        }
    while (1){
        rc = zmq_msg_init (&msg);
        if (rc != 0) {
          printf ("Main thread error in zmq_msg_init: %s\n", zmq_strerror (errno));
          return 1;
          }
        rc=zmq_msg_recv(&msg, s, 0);
        if (rc < 0) {
            printf ("Main thread error in zmq_recvmsg: %s\n",zmq_strerror (errno));
            return 1;
        }
        char *msgStr = (char*) zmq_msg_data(&msg);
        if (strcmp(msgStr, "TERMINATE") == 0)
        {
            printf("Received terminate message by Main Thread %s \n",msgStr);
            zmq_msg_close(&msg);
            break;
        }
        message_size = zmq_msg_size(&msg);
        received_bytes += message_size;
        message_count++;
        Message* message = malloc(sizeof(Message));
        message->size = message_size;
        message->data = malloc(message_size);
        memcpy(message->data, msgStr, message_size);
        rc = zmq_msg_close (&msg);
        if (rc != 0) {
          printf ("Main thread error in zmq_msg_close: %s\n", zmq_strerror (errno));
          return 1;
        }
        queue_push(queue, message);
    }
    rc = zmq_close (s);
    if (rc != 0) {
        printf ("Main thread error in zmq_close: %s\n", zmq_strerror (errno));
        return 1;
    }
    rc = zmq_ctx_term (ctx);
    if (rc != 0) {
        printf ("Main thread error in zmq_ctx_term: %s\n", zmq_strerror (errno));
        return 1;
    }
    pthread_mutex_lock(&total_bytes_lock);
    total_bytes += received_bytes;
    total_message_count += message_count;
    pthread_mutex_unlock(&total_bytes_lock);

    for(int i = 1; i < num_receiver_threads; i++) {
        pthread_join(receive_threads[i], NULL);
    }
    for(int i = 0; i < num_decompression_threads; i++) {
        pthread_join(decompression_threads[i], NULL);
    }
    elapsed = zmq_stopwatch_stop (watch);
    if (elapsed == 0)
        elapsed = 1;

    message_size = total_bytes/total_message_count; // convert message size to bits
    throughput = ((double) total_message_count / (double) elapsed * 1000000);
    megabits = ((double) throughput * message_size * 8) / 1000000;

    printf ("message size: %d [B]\n", (int) message_size);
    printf ("message count: %d\n", (int) total_message_count);
    printf ("mean Network throughput: %d [msg/s]\n", (int) throughput);
    printf ("mean Network throughput: %.3f [Mb/s]\n", (double) megabits);
    printf ("total bytes received: %lu [B]\n", total_bytes); // print the total bytes received
    printf("Queue size: %d\n", queue_size(queue));
    printf("Total decompressed size: %lu\n", total_decompressed_size);
    message_size = total_decompressed_size/total_message_count; // convert message size to bits
    throughput = ((double) total_message_count / (double) elapsed * 1000000);
    megabits = ((double) throughput * message_size * 8) / 1000000;
    printf ("Decompressed message size: %d [B]\n", (int) message_size);
    printf ("Decompressed message count: %d\n", (int) total_message_count);
    printf ("Decompressed mean throughput: %d [msg/s]\n", (int) throughput);
    printf ("Decompressed mean throughput: %.3f [Mb/s]\n", (double) megabits);
    // double decompression_throughput = (double)total_decompressed_size / (double)elapsed;
    // double decompression_throughput_gbps= (double)decompression_throughput * 8 / 1000000000;
    for (i = 0; i < num_receiver_threads; i++) {
        free(bind_to[i]);
    }
    free(bind_to);

    return 0;


}
