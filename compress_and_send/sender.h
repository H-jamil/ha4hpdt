#ifndef SENDER_H
#define SENDER_H
#include "/home/beams/MJAMIL/project/libzmq/include/zmq.h"
#include "queue.h"
#include "data_generator.h"
#include "reader.h"
#include <errno.h>
#include <unistd.h>

typedef struct {
    char *connect_to;
    int thread_id;
    int numa_node;
    Queue* queue;
} send_thread_args;

void* dequeue_and_send(void* args);

#endif // SENDER_H
