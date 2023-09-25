#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <H5public.h>
#include <H5Fpublic.h>
#include <H5Dpublic.h>
#include <H5Spublic.h>
#include <H5Ppublic.h>
#include <H5Tpublic.h>
#include <pthread.h>
#include <stdint.h>
#include <hdf5.h>
#include <string.h>
#include "/home/beams/MJAMIL/project/libzmq/include/zmq.h"
#include "lz4.h" // make sure LZ4 is included
#include <sched.h>
#include <numa.h>
#include <assert.h>
#include <numaif.h>
#include <unistd.h>
#include "queue.h"


bool consume_flag = true;


typedef struct {
    char *data;
    size_t size;
    size_t chunk_size;
    size_t current_index;
    int call_number;
    pthread_mutex_t lock;
} DataGenerator;

DataGenerator* create_generator(char *data, size_t size, size_t chunk_size) {
    DataGenerator *generator = malloc(sizeof(DataGenerator));
    generator->data = data;
    generator->size = size;
    generator->chunk_size = chunk_size;
    generator->current_index = 0;
    pthread_mutex_init(&generator->lock, NULL);
    return generator;
}

char* next_chunk(DataGenerator *generator) {
    pthread_mutex_lock(&generator->lock);

    if (generator->current_index >= generator->size) {
        pthread_mutex_unlock(&generator->lock);
        return NULL;
    }

    char *chunk_start = &generator->data[generator->current_index];
    generator->current_index += generator->chunk_size;

    pthread_mutex_unlock(&generator->lock);
    return chunk_start;
}

void destroy_generator(DataGenerator *generator) {
    pthread_mutex_destroy(&generator->lock);
    numa_free(generator->data, generator->size);  // Frees the memory
    free(generator);
}


typedef struct {
    char* data;
    int len;
} Chunk;

typedef struct {
    DataGenerator *generator;
    int thread_id;
    Queue* queue;
    int num_consumer_threads;
} prodThreadData;

typedef struct {
    int thread_id;
    Queue* queue;
} conThreadData;

int get_memory_allocation_node(void *p)
{
  int node = -1;
  int ret = get_mempolicy(&node, NULL, 0, p, MPOL_F_NODE | MPOL_F_ADDR);
  if (ret < 0) {
    printf("Error getting memory allocation node\n");
  } else {
    printf("Memory is in node %d\n", node);
  }
    return node;
}


char* read_hdf5(const char* filename, size_t *data_size, int numa_node) {
    printf("Reading data from file: %s\n", filename);

    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);

    hid_t file_id, dataset_id, dataspace_id;
    hsize_t dims[3];

    printf("Opening file: %s\n", filename);
    file_id = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);

    printf("Opening dataset: exchange/data\n");
    dataset_id = H5Dopen2(file_id, "exchange/data", H5P_DEFAULT);

    dataspace_id = H5Dget_space(dataset_id);

    H5Sget_simple_extent_dims(dataspace_id, dims, NULL);
    *data_size = dims[0] * dims[1] * dims[2] * sizeof(uint16_t); // Save the size to the provided variable

    printf("Allocating memory for data on NUMA node %d...\n", numa_node);

    struct bitmask *nodemask = numa_allocate_nodemask();
    numa_bitmask_clearall(nodemask);
    numa_bitmask_setbit(nodemask, numa_node);
    numa_set_membind(nodemask);
    numa_free_nodemask(nodemask);
    char *data = (char*) numa_alloc_onnode(dims[0] * dims[1] * dims[2] * sizeof(uint16_t), numa_node);
    get_memory_allocation_node(data);
    printf("Reading data...\n");
    // H5Dread(dataset_id, H5T_NATIVE_UINT16, H5S_ALL, H5S_ALL, H5P_DEFAULT, data);

    H5Sclose(dataspace_id);
    H5Dclose(dataset_id);
    H5Fclose(file_id);

    gettimeofday(&end_time, NULL);
    long elapsed_read = ((end_time.tv_sec - start_time.tv_sec) * 1000) +
                        ((end_time.tv_usec - start_time.tv_usec) / 1000);

    printf("Read time: %ld milliseconds\n", elapsed_read);
    printf("Size of the HDF5 file (in bytes): %zu\n", dims[0] * dims[1] * dims[2] * sizeof(uint16_t));
    return data;
}


// void* producer_thread(void *arg) {
//     prodThreadData *data = (prodThreadData*)arg;
//     char *chunk;
//     int no_chunks_read=0;
//     DataGenerator *generator = data->generator;
//     int dest_size=LZ4_compressBound(generator->chunk_size);
//     while ((chunk = next_chunk(generator)) != NULL) {
//         // Print chunk size
//         char* dest = malloc(dest_size);
//         if (dest == NULL) {
//             printf("Failed to allocate memory for compression.\n");
//             free(arg);
//             pthread_exit(NULL);
//         }
//         size_t actual_data_size = generator->chunk_size;
//         if (generator->current_index > generator->size - generator->chunk_size) {
//           // Needs to be looked at more details
//             actual_data_size = generator->size - generator->current_index;
//             // break;
//         }
//         int compressedSize = LZ4_compress_default(chunk, dest, actual_data_size, dest_size);
//         if (compressedSize < 0) {
//             printf("Failed to compress data\n");
//             free(dest);
//             free(arg);
//             pthread_exit(NULL);
//         }
//         no_chunks_read+=1;
//         Chunk* qchunk = malloc(sizeof(Chunk));
//         qchunk->data = dest;
//         qchunk->len = compressedSize;
//         queue_push(data->queue, qchunk);
//         pthread_cond_signal(&data_ready);
//     }
//     printf("Thread %d read total %d chunks.\n", data->thread_id,no_chunks_read);
//     pthread_mutex_lock(&prod_finished_mutex);
//     prod_finished++;
//     pthread_mutex_unlock(&prod_finished_mutex);
//     pthread_cond_broadcast(&prod_finished_cond);
//     free(arg);
//     pthread_exit(NULL);
// }

void* producer_thread(void *arg) {
    prodThreadData *data = (prodThreadData*)arg;
    char *chunk;
    int no_chunks_read = 0;
    DataGenerator *generator = data->generator;
    int dest_size = LZ4_compressBound(generator->chunk_size);

    while ((chunk = next_chunk(generator)) != NULL) {
        char* dest = malloc(dest_size);
        if (dest == NULL) {
            printf("Failed to allocate memory for compression.\n");
            free(arg);
            pthread_exit(NULL);
        }
        size_t actual_data_size = generator->chunk_size;
        if (generator->current_index > generator->size - generator->chunk_size) {
            actual_data_size = generator->size - generator->current_index;
        }
        int compressedSize = LZ4_compress_default(chunk, dest, actual_data_size, dest_size);
        if (compressedSize < 0) {
            printf("Failed to compress data\n");
            free(dest);
            free(arg);
            pthread_exit(NULL);
        }
        no_chunks_read += 1;
        Chunk* qchunk = malloc(sizeof(Chunk));
        qchunk->data = dest;
        qchunk->len = compressedSize;
        queue_push(data->queue, qchunk);
    }
    printf("Thread %d read total %d chunks.\n", data->thread_id, no_chunks_read);
    free(arg);

    for (int i = 0; i < data->num_consumer_threads; i++) {
        queue_push(data->queue, NULL);
    }
    pthread_exit(NULL);
}



void* consumer_thread(void* arg) {
    conThreadData *params = (conThreadData*) arg;
    int no_chunks_send=0;
    // printf("Send thread %d starting...\n", params->thread_id);
    while (1){
        Chunk *chunk =(Chunk*) queue_pop(params->queue);
        // printf("Thread %d sending chunk of size %d\n", params->thread_id, chunk->len);
        if (chunk == NULL) {
            printf("Send thread %d exiting with send count %d...\n", params->thread_id,no_chunks_send);
            pthread_exit(NULL);
        }
        // printf("Thread %d sending chunk of size %d\n", params->thread_id, chunk->len);
        // Initialize a zmq_msg_t with the size of the chunk
        free(chunk->data);
        free(chunk);
        no_chunks_send+=1;
    }
    // free(arg->ip_port);
    printf("Send thread %d exiting with send count %d...\n", params->thread_id,no_chunks_send);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <filename> <#compressionthreads> <#sendthreads> \n", argv[0]);
        return 1;
    }

    char *filename = argv[1];
    int num_threads = atoi(argv[2]);
    int num_send_threads = atoi(argv[3]);
    size_t data_size;
    int memory_node = 1; // adjust as needed
    // insert the read_hdf5 function here
    char *data = read_hdf5(filename,&data_size,memory_node);

    size_t chunk_size = 1658880;  // adjust as needed

    DataGenerator *generator = create_generator(data, data_size, chunk_size);

    pthread_t *Prodthreads = malloc(sizeof(pthread_t) * num_threads);
    pthread_t *Cond_threads = malloc(sizeof(pthread_t) * num_send_threads);
    Queue *queue=queue_create();
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);  // start timing
    for (int i = 0; i < num_threads; i++) {
        prodThreadData *p_data = malloc(sizeof(prodThreadData));
        p_data->generator = generator;
        p_data->thread_id = i;
        p_data->queue = queue;
        p_data->num_consumer_threads = num_send_threads;
        if (pthread_create(&Prodthreads[i], NULL, producer_thread, p_data) != 0) {
            printf("Failed to create compress thread %d.\n", i);
            return 1;
        }
    }
    for (int i = 0; i < num_send_threads; i++) {
        conThreadData *c_data = malloc(sizeof(conThreadData));
        c_data->thread_id = i;
        c_data->queue = queue;
        if (pthread_create(&Cond_threads[i], NULL, consumer_thread, c_data) != 0) {
            printf("Failed to create send thread %d.\n", i);
            return 1;
        }
        // printf("Created send thread with ip port %s\n", remote_ip_port);
    }
    for (int i = 0; i < num_threads; i++) {
        if (pthread_join(Prodthreads[i], NULL) != 0) {
            printf("Failed to join thread %d.\n", i);
            return 1;
        }
    }
    consume_flag = false;
    for (int i = 0; i < num_send_threads; i++) {
        if (pthread_join(Cond_threads[i], NULL) != 0) {
            printf("Failed to join thread %d.\n", i);
            return 1;
        }
    }
    gettimeofday(&end_time, NULL);  // end timing

    long elapsed_time = ((end_time.tv_sec - start_time.tv_sec) * 1000) +
                        ((end_time.tv_usec - start_time.tv_usec) / 1000);
    // pthread_cancel(queue_monitor);
    // pthread_join(queue_monitor, NULL);
    double sending_throughput = (data_size / elapsed_time);
    double sending_throuhgput_gbps =sending_throughput * 8 / 0.001 / 1e9;
    printf("Elapsed time: %ld milliseconds with %.6f Gbps\n", elapsed_time, sending_throuhgput_gbps);

    destroy_generator(generator);

    free(Prodthreads);
    free(Cond_threads);

    return 0;
}
