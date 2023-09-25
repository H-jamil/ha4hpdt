// producer_thread function
void* producer_thread(void *arg) {
    // (existing code...)
    while ((chunk = next_chunk(generator)) != NULL) {
        // (existing code...)
        queue_push(data->queue, qchunk);
        // signal to a consumer that data is ready to consume
        pthread_cond_signal(&data_ready);
    }
    printf("Thread %d read total %d chunks.\n", data->thread_id,no_chunks_read);
    // indicate producer has finished producing data
    pthread_mutex_lock(&prod_finished_mutex);
    prod_finished++;
    pthread_mutex_unlock(&prod_finished_mutex);
    // signal to all consumers to check if all producers have finished
    pthread_cond_broadcast(&prod_finished_cond);
    free(arg);
    printf("Producer thread %d exiting...\n", data->thread_id);
    pthread_exit(NULL);
}

// consumer_thread function
void* consumer_thread(void* arg) {
    conThreadData *params = (conThreadData*) arg;
    int no_chunks_send=0;
    printf("Send thread %d starting...\n", params->thread_id);
    while (1) {
        pthread_mutex_lock(&queue_mutex);
        // wait until there is data in the queue or all producers have finished
        while (queue_size(params->queue) == 0) {
            pthread_cond_wait(&data_ready, &queue_mutex);
            // if all producers have finished and there's no data left to consume
            pthread_mutex_lock(&prod_finished_mutex);
            if (prod_finished == num_threads && queue_size(params->queue) == 0) {
                pthread_mutex_unlock(&prod_finished_mutex);
                pthread_mutex_unlock(&queue_mutex);
                printf("Send thread %d exiting with send count %d...\n", params->thread_id,no_chunks_send);
                pthread_exit(NULL);
            }
            pthread_mutex_unlock(&prod_finished_mutex);
        }
        Chunk *chunk =(Chunk*) queue_pop(params->queue);
        pthread_mutex_unlock(&queue_mutex);
        // (existing code...)
    }
}

// main function
int main(int argc, char *argv[]) {
    // (existing code...)
    // initializing synchronization primitives
    pthread_mutex_init(&queue_mutex, NULL);
    pthread_mutex_init(&prod_finished_mutex, NULL);
    pthread_cond_init(&data_ready, NULL);
    pthread_cond_init(&prod_finished_cond, NULL);
    prod_finished = 0;
    // (existing code...)
    pthread_mutex_destroy(&queue_mutex);
    pthread_mutex_destroy(&prod_finished_mutex);
    pthread_cond_destroy(&data_ready);
    pthread_cond_destroy(&prod_finished_cond);
    // (existing code...)
    return 0;
}
