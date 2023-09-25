#ifndef QUEUE_H
#define QUEUE_H

typedef struct Node Node;
typedef struct Queue Queue;

Queue* queue_create();
void queue_destroy(Queue* queue);
void queue_push(Queue* queue, void* data);
void* queue_pop(Queue* queue);
int queue_size(Queue* queue);

#endif
