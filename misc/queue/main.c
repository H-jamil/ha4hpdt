#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include "queue.h"
#include<unistd.h>

#define PRODUCER_THREAD_COUNT 5
#define CONSUMER_THREAD_COUNT 5
#define MAX_ITEMS 100

Queue *queue;
// volatile int running = 1;
void *producer(void *arg) {
    int count=0;
    for (int i = 0; i < MAX_ITEMS; i++) {
        int *value = malloc(sizeof(int));
        *value = rand() % 100;
        queue_push(queue, value);
        // printf("Produced %d\n", *value);
        count++;
    }
    printf("Produced %d items\n", count);
    pthread_exit(NULL);
}

void *consumer(void *arg) {
    int count=0;
    while (queue_size(queue) > 0) {
        int *value = (int *) queue_pop(queue);
        free(value);
        count++;
    }
    printf("Consumed %d items\n", count);
    pthread_exit(NULL);
}

void *monitor(void *arg) {
    while (1) {
        sleep(1);
        time_t now;
        time(&now);
        printf("[%s] Current queue size: %d\n", ctime(&now), queue_size(queue));
    }
    pthread_exit(NULL);
}

int main() {
    queue = queue_create();

    pthread_t producer_threads[PRODUCER_THREAD_COUNT];
    pthread_t consumer_threads[CONSUMER_THREAD_COUNT];
    pthread_t monitor_thread;

    for (int i = 0; i < PRODUCER_THREAD_COUNT; i++) {
        pthread_create(&producer_threads[i], NULL, producer, NULL);
    }

    for (int i = 0; i < CONSUMER_THREAD_COUNT; i++) {
        pthread_create(&consumer_threads[i], NULL, consumer, NULL);
    }

    pthread_create(&monitor_thread, NULL, monitor, NULL);

    for (int i = 0; i < PRODUCER_THREAD_COUNT; i++) {
        pthread_join(producer_threads[i], NULL);
    }
    sleep(1);
    // running = 0;
    for (int i = 0; i < CONSUMER_THREAD_COUNT; i++) {
        pthread_join(consumer_threads[i], NULL);
    }

    pthread_cancel(monitor_thread);
    pthread_join(monitor_thread, NULL);

    queue_destroy(queue);

    return 0;
}
