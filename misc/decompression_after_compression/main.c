#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <hdf5.h>
#include <pthread.h>
#include <unistd.h>
#include "reader.h"
#include "data_generator.h"
#include "queue.h"
#include "decompression.h"

// #define CHUNK_SIZE 5529600
size_t total_decompressed_size=0;
pthread_mutex_t total_decompressed_size_lock;

int main(int argc, char* argv[]) {
    if (argc != 9) { //       1              2                  3                   4               5         6                  7                      8
        printf("Usage: %s <hdf5_file><#compressionthreads><memory_node><compression_node><file_multiplier><queue_node><#decompression_threads><decompression_node>\n", argv[0]);
        return 1;
    }
    const char* filename = argv[1];
    size_t* data_size = malloc(sizeof(size_t));
    size_t* z_dim=malloc(sizeof(size_t));
    size_t* y_dim=malloc(sizeof(size_t));
    size_t* x_dim=malloc(sizeof(size_t));
    int memory_node = atoi(argv[3]);
    int queue_mem_node = atoi(argv[6]);
    int num_decompression_threads = atoi(argv[7]);
    int decompression_thread_node = atoi(argv[8]);
    uint16_t* data = read_hdf5(filename,data_size,memory_node,z_dim,y_dim,x_dim);
    if (data == NULL) {
        printf("Error reading data from hdf5 file\n");
        return 1;
    }
    int file_multiplier = atoi(argv[5]);
    size_t actual_data_size = *data_size * file_multiplier;
    printf("Data size: %zu\n", *data_size);
    // DataGenerator *gen = data_generator_init(data, data_size, CHUNK_SIZE, memory_node);
    DataGenerator *generator = data_generator_init(data, *data_size, *y_dim * *x_dim * sizeof(uint16_t),memory_node,file_multiplier);
    if (generator == NULL) {
        printf("Error initializing data generator\n");
        return 1;
    }
    Queue *queue=queue_create();
    int compression_thread_node = atoi(argv[4]);
    int num_compression_threads = atoi(argv[2]);
    struct timeval start_time, end_time;
    pthread_t *compression_threads = malloc(sizeof(pthread_t) * num_compression_threads);
    gettimeofday(&start_time, NULL);  // start timing
    for (int i = 0; i < num_compression_threads; i++) {
        prodThreadData *c_t_data = malloc(sizeof(prodThreadData));
        c_t_data->generator = generator;
        c_t_data->thread_id = i;
        c_t_data->queue = queue;
        c_t_data->queue_memory_location=queue_mem_node;
        // c_t_data->num_consumer_threads = num_sending_threads;
        c_t_data->numa_node = compression_thread_node;
        // if ( i % 2 == 0) {
        //     c_t_data->numa_node = compression_thread_node;
        // }
        // else{
        //     c_t_data->numa_node = compression_thread_node^1;
        // }
        if (pthread_create(&compression_threads[i], NULL, reader_thread, c_t_data) != 0) {
            printf("Failed to create compress thread %d.\n", i);
            return 1;
        }
    }

    for (int i = 0; i < num_compression_threads; i++) {
        if (pthread_join(compression_threads[i], NULL) != 0) {
            printf("Failed to join thread %d.\n", i);
            return 1;
        }
    }
    gettimeofday(&end_time, NULL);  // end timing

    long elapsed_time = ((end_time.tv_sec - start_time.tv_sec) * 1000) +
                        ((end_time.tv_usec - start_time.tv_usec) / 1000);

    printf("Elapsed time in generating: %ld milliseconds\n", elapsed_time);
    printf("generator->call_number: %d\n", generator->current_call_number);
    double sending_throughput = (actual_data_size / elapsed_time);
    double sending_throuhgput_gbps =sending_throughput * 8 / 0.001 / 1e9;
    printf("Elapsed time: %ld milliseconds with %.6f Gbps\n", elapsed_time, sending_throuhgput_gbps);
    printf("Queue size: %d\n", queue_size(queue));
    printf("Compression finished and now start decompression\n");
    for(int i = 0; i < num_decompression_threads;i++){
         queue_push(queue, NULL);
    }
    printf("Queue size after main putting NULLs : %d\n", queue_size(queue));
    // while(queue_size(queue)> 0){
    //     Chunk *chunk =(Chunk*) queue_pop(queue);
    //     if(chunk==NULL){
    //         printf("Error in popping chunk from queue\n");
    //         return 1;
    //     }
    //     numa_free(chunk->data,chunk->len);
    // }
    // printf("Queue size: %d\n", queue_size(queue));
    decompression_thread_args decompression_args[num_decompression_threads];
    pthread_t decompression_threads[num_decompression_threads];
    pthread_mutex_init(&total_decompressed_size_lock, NULL);

    gettimeofday(&start_time, NULL);  // start timing
    for(int i = 0; i < num_decompression_threads; i++) {
        decompression_args[i].thread_id = i;
        // decompression_args[i].numa_node = decompression_thread_node;
        if ( i % 2 == 0) {
            decompression_args[i].numa_node = decompression_thread_node;
        }
        else{
            decompression_args[i].numa_node = decompression_thread_node^1;
        }
        decompression_args[i].queue = queue;
        decompression_args[i].chunk_size = *y_dim * *x_dim * sizeof(uint16_t);
        pthread_create(&decompression_threads[i], NULL, dequeue_and_decompress, (void*)&decompression_args[i]);
    }
    for(int i = 0; i < num_decompression_threads; i++) {
        pthread_join(decompression_threads[i], NULL);
    }
    gettimeofday(&end_time, NULL);  // end timing

    elapsed_time = ((end_time.tv_sec - start_time.tv_sec) * 1000) +
                        ((end_time.tv_usec - start_time.tv_usec) / 1000);

    printf("Elapsed time in decompressing: %ld milliseconds\n", elapsed_time);
    double decompressing_throughput = (total_decompressed_size / elapsed_time);
    double decompressing_throughput_gbps =decompressing_throughput * 8 / 0.001 / 1e9;
    printf("Elapsed decompressing time: %ld milliseconds with %.6f Gbps\n", elapsed_time, decompressing_throughput_gbps);

    queue_destroy(queue);
    data_generator_free(generator);
    numa_free(data, *data_size);
    return 0;
}
