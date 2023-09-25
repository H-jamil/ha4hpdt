#ifndef DECOMPRESSION_H
#define DECOMPRESSION_H

#include "queue.h"
#include "receiver.h"
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
    int thread_id;
    int numa_node;
    Queue* queue;
    int chunk_size;
} decompression_thread_args;

void* dequeue_and_decompress(void* args);

#endif // DECOMPRESSION_H


