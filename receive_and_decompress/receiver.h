#ifndef RECEIVER_H
#define RECEIVER_H

#include "/home/beams/MJAMIL/project/libzmq/include/zmq.h"
#include "queue.h"
#include <pthread.h>
#include <stdint.h>
#include <hdf5.h>
#include <sched.h>
#include <numa.h>
#include <assert.h>
#include <numaif.h>
#include <unistd.h>
#include <sys/time.h>


typedef struct {
    char *bind_to;
    int thread_id;
    int numa_node;
    Queue* queue;
    int num_decompression_threads;
} receive_thread_args;

typedef struct Message {
    char *data;
    size_t size;
} Message;

void* thread_receive(void *args);
#endif // RECEIVER_H
