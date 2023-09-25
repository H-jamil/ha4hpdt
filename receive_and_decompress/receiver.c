#include "receiver.h"
#include "/home/beams/MJAMIL/project/libzmq/include/zmq.h"
#include "queue.h"
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <hdf5.h>
#include <sched.h>
#include <numa.h>
#include <assert.h>
#include <numaif.h>
#include <unistd.h>
#include <sys/time.h>

extern size_t total_bytes;
extern int total_message_count;
extern pthread_mutex_t total_bytes_lock;
extern int queue_null_flag;
extern pthread_mutex_t queue_null_flag_lock;

void* thread_receive(void *args) {
    receive_thread_args *params = (receive_thread_args*) args;
    // struct bitmask *nodemask = numa_allocate_nodemask();
    // numa_bitmask_clearall(nodemask);
    // numa_bitmask_setbit(nodemask, params->numa_node);
    // numa_bind(nodemask);
    // numa_free_nodemask(nodemask);
    int rc;
    zmq_msg_t msg;
    int message_count;
    void *s;
    void *ctx;
    size_t received_bytes = 0;
    ctx = zmq_init (1);
    int message_size=0;
    if (!ctx) {
        printf ("Thread id %d error in zmq_init: %s\n", params->thread_id, zmq_strerror (errno));
        pthread_exit(NULL);
    }
    s = zmq_socket (ctx, ZMQ_PULL);
    if (!s) {
        printf ("Thread id %d error in zmq_socket: %s\n", params->thread_id, zmq_strerror (errno));
        pthread_exit(NULL);
    }

    rc = zmq_bind (s, params->bind_to);
    if (rc != 0) {
        printf ("Thread id %d error in zmq_bind: %s\n",params->thread_id, zmq_strerror (errno));
        pthread_exit(NULL);
    }
    printf ("Thread %d listening to %s\n", params->thread_id,params->bind_to);
    while (1){
        rc = zmq_msg_init (&msg);
        if (rc != 0) {
          printf ("Thread %d error in zmq_msg_init: %s\n",params->thread_id, zmq_strerror (errno));
          pthread_exit(NULL);
        }
        rc=zmq_msg_recv(&msg, s, 0);
        if (rc < 0) {
            printf ("Thread id %d error in zmq_recvmsg: %s\n", params->thread_id,zmq_strerror (errno));
            pthread_exit(NULL);
        }
        char *msgStr = (char*) zmq_msg_data(&msg);
        if (strcmp(msgStr, "TERMINATE") == 0)
        {
            printf("Received terminate message by thread %d %s \n",params->thread_id,msgStr);
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
          printf ("Thread id %d error in zmq_msg_close: %s\n", params->thread_id,zmq_strerror (errno));
          pthread_exit(NULL);
        }
        queue_push(params->queue, message);
    }
    rc = zmq_close (s);
    if (rc != 0) {
        printf ("Thread id %d error in zmq_close: %s\n",params->thread_id, zmq_strerror (errno));
        pthread_exit(NULL);
    }
    rc = zmq_ctx_term (ctx);
    if (rc != 0) {
        printf ("Thread id %d error in zmq_ctx_term: %s\n",params->thread_id, zmq_strerror (errno));
        pthread_exit(NULL);
    }
    if (queue_null_flag == 0) {
        pthread_mutex_lock(&queue_null_flag_lock);
        if (queue_null_flag == 0) {
            queue_null_flag = 1;
            for (int i = 0; i < params->num_decompression_threads; i++) {
                queue_push(params->queue, NULL);
                }
        }
        pthread_mutex_unlock(&queue_null_flag_lock);
    }

    pthread_mutex_lock(&total_bytes_lock);
    total_bytes += received_bytes;
    total_message_count += message_count;
    pthread_mutex_unlock(&total_bytes_lock);
    pthread_exit(NULL);
}

