#ifndef READER_H
#define READER_H

#include "data_generator.h"
#include <stdio.h>
#include "lz4.h" // make sure LZ4 is included
#include <string.h>
#include <stdlib.h>
#include "queue.h"

typedef struct {
    DataGenerator *generator;
    int thread_id;
    int numa_node;
    int queue_memory_location;  // 0 for local, 1 for remote
    Queue *queue;
    int num_consumer_threads;
} prodThreadData;

typedef struct {
    char* data;
    int len;
} Chunk;

void* reader_thread(void *arg);

#endif // READER_H
