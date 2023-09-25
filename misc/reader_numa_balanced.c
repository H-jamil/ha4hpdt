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


int get_memory_allocation_node(void *p)
{
  int node = -1;
  int ret = get_mempolicy(&node, NULL, 0, p, MPOL_F_NODE | MPOL_F_ADDR);
  if (ret < 0) {
    printf("Error getting memory allocation node\n");
  } else {
    printf("Memory is in node %d\n", node);
  }
}

typedef struct {
    char *data;
    size_t size;
    size_t chunk_size;
    size_t current_index;
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
    // free(generator->data);
    numa_free(generator->data, generator->size);  // Frees the memory
    free(generator);
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

    printf("Allocating memory for data on NUMA node %d...\n", numa_node);
    *data_size = dims[0] * dims[1] * dims[2] * sizeof(uint16_t); // Save the size to the provided variable

    struct bitmask *nodemask = numa_allocate_nodemask();
    numa_bitmask_clearall(nodemask);
    numa_bitmask_setbit(nodemask, numa_node);
    numa_set_membind(nodemask);
    numa_free_nodemask(nodemask);
    char *data = (char*) numa_alloc_onnode(dims[0] * dims[1] * dims[2] * sizeof(uint16_t), numa_node);
    // printf("allocated data is on node %d\n",get_memory_allocation_node(data));
    get_memory_allocation_node(data);
    printf("Reading data...\n");
    H5Dread(dataset_id, H5T_NATIVE_UINT16, H5S_ALL, H5S_ALL, H5P_DEFAULT, data);

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

typedef struct {
    DataGenerator *generator;
    int thread_id;
    int numa_node;
} ThreadData;

void* reader_thread(void *arg) {
    ThreadData *data = (ThreadData*)arg;
    struct bitmask *nodemask = numa_allocate_nodemask();
    numa_bitmask_clearall(nodemask);
    numa_bitmask_setbit(nodemask, data->numa_node);
    numa_bind(nodemask);
    numa_free_nodemask(nodemask);
    DataGenerator *generator = data->generator;
    char *chunk;
    int no_chunks_read=0;
    int dest_size=LZ4_compressBound(generator->chunk_size);
    char* dest = malloc(dest_size);
        if (dest == NULL) {
            printf("Failed to allocate memory for compression.\n");
            free(arg);
            pthread_exit(NULL);
        }
    while ((chunk = next_chunk(generator)) != NULL) {
        size_t actual_data_size = generator->chunk_size;
        if (generator->current_index > generator->size - generator->chunk_size) {
            actual_data_size = generator->size % generator->chunk_size;
        }
        int compressedSize = LZ4_compress_default(chunk, dest, actual_data_size, dest_size);
        // printf("Thread %d compressed a chunk of size %zu to %d.\n", data->thread_id, actual_data_size, compressedSize);
        if (compressedSize < 0) {
            printf("Failed to compress data\n");
            free(dest);
            free(arg);
            pthread_exit(NULL);
        }
        no_chunks_read+=1;
    }
    printf("Thread %d read total %d chunks.\n", data->thread_id,no_chunks_read);
    free(arg);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s filename num_threads memory_node thread_node\n", argv[0]);
        return 1;
    }

    char *filename = argv[1];
    int num_threads = atoi(argv[2]);
    int memory_node = atoi(argv[3]);
    int thread_node = atoi(argv[4]);
    size_t data_size;
    // insert the read_hdf5 function here
    char *data = read_hdf5(filename,&data_size,memory_node);


    size_t chunk_size = 1658880;  // adjust as needed
    DataGenerator *generator = create_generator(data, data_size, chunk_size);

    pthread_t *threads = malloc(sizeof(pthread_t) * num_threads);
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);  // start timing
    for (int i = 0; i < num_threads; i++) {
        ThreadData *data = malloc(sizeof(ThreadData));
        data->generator = generator;
        data->thread_id = i;
        if (i <16) {
            data->numa_node = thread_node;
        }
        else{
            data->numa_node = thread_node^1;
        }

        if (pthread_create(&threads[i], NULL, reader_thread, data) != 0) {
            printf("Failed to create thread %d.\n", i);
            return 1;
        }
    }

    for (int i = 0; i < num_threads; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            printf("Failed to join thread %d.\n", i);
            return 1;
        }
    }
    gettimeofday(&end_time, NULL);  // end timing

    long elapsed_time = ((end_time.tv_sec - start_time.tv_sec) * 1000) +
                        ((end_time.tv_usec - start_time.tv_usec) / 1000);
    printf("Elapsed time: %ld milliseconds\n", elapsed_time);
    destroy_generator(generator);
    free(threads);
    return 0;
}
