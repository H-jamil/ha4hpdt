#include "reader.h"

void* reader_thread(void *arg) {
    prodThreadData *data = (prodThreadData*)arg;
    // struct bitmask *nodemask = numa_allocate_nodemask();
    // numa_bitmask_clearall(nodemask);
    // numa_bitmask_setbit(nodemask, data->numa_node);
    // numa_bind(nodemask);
    // numa_free_nodemask(nodemask);
    DataGenerator *generator = data->generator;
    uint16_t *chunk;
    int no_chunks_read=0;
    size_t size;
    int queue_mem_loc=data->queue_memory_location;
    while ((chunk = data_generator_next(generator,&size)) != NULL) {
        int dest_size=LZ4_compressBound(size);
        // char* dest = malloc(dest_size);
        char* dest = numa_alloc_onnode(dest_size,queue_mem_loc);
        if (dest == NULL) {
            printf("Failed to allocate memory for compression.\n");
            free(arg);
            pthread_exit(NULL);
        }
        int compressedSize = LZ4_compress_default((const char*)chunk, dest, size, dest_size);
        // printf("Thread %d compressed a chunk of size %zu to %d.\n", data->thread_id, size, compressedSize);
        // Thread 1 compressed a chunk of size 11059200 to 11102333.
        if (compressedSize < 0) {
            printf("Failed to compress data\n");
            // free(dest);
            numa_free(dest,dest_size);
            free(arg);
            pthread_exit(NULL);
        }
        no_chunks_read+=1;
        // free(dest);
        Chunk* qchunk = malloc(sizeof(Chunk));
        qchunk->data = dest;
        qchunk->len = compressedSize;
        queue_push(data->queue, qchunk);
    }
    printf("Thread %d read total %d chunks.\n", data->thread_id, no_chunks_read);
    // for (int i = 0; i < data->num_consumer_threads; i++) {
    //     queue_push(data->queue, NULL);
    // }
    free(arg);
    pthread_exit(NULL);
}
