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

typedef struct {
    uint16_t *data;
    size_t size;
    size_t chunk_size;
    size_t current_index;
    pthread_mutex_t lock;
} DataGenerator;

DataGenerator* create_generator(uint16_t *data, size_t size, size_t chunk_size) {
    DataGenerator *generator = (DataGenerator*) malloc(sizeof(DataGenerator));
    if(generator == NULL) {
        perror("Failed to allocate memory for DataGenerator");
        exit(EXIT_FAILURE);
    }
    generator->data = data;
    generator->size = size;
    generator->chunk_size = chunk_size;
    generator->current_index = 0;
    pthread_mutex_init(&generator->lock, NULL);
    return generator;
}

uint16_t* next_chunk(DataGenerator *generator) {
    pthread_mutex_lock(&generator->lock);

    if (generator->current_index >= generator->size) {
        pthread_mutex_unlock(&generator->lock);
        return NULL;
    }

    uint16_t *chunk_start = &generator->data[generator->current_index];
    generator->current_index += generator->chunk_size;

    pthread_mutex_unlock(&generator->lock);
    return chunk_start;
}

void destroy_generator(DataGenerator *generator) {
    pthread_mutex_destroy(&generator->lock);
    free(generator->data);
    free(generator);
}


uint16_t* read_hdf5(const char* filename, size_t *data_size) {
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

    printf("Allocating memory for data...\n");
    *data_size = dims[0] * dims[1] * dims[2] * sizeof(uint16_t); // Save the size to the provided variable
    uint16_t *data = (uint16_t*) malloc(dims[0] * dims[1] * dims[2] * sizeof(uint16_t));

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
} ThreadData;


void* reader_thread(void *arg) {
    ThreadData *data = (ThreadData*)arg;
    DataGenerator *generator = data->generator;
    uint16_t *chunk;
    int no_chunks_read=0;
    size_t actual_data_size = generator->chunk_size;
    // You need to estimate the maximum compressed size and allocate the memory accordingly
    // LZ4_COMPRESSBOUND provides an upper bound on the size of the compressed data
    char *dest = malloc(LZ4_COMPRESSBOUND(actual_data_size * sizeof(uint16_t)));
    if (dest == NULL) {
        printf("Failed to allocate memory for compression.\n");
        free(arg);
        return NULL;
    }

    while ((chunk = next_chunk(generator)) != NULL) {
        // Calculate the actual amount of data left to compress
        if (generator->current_index > generator->size-generator->chunk_size) {
            actual_data_size = generator->size % generator->chunk_size;
        }
        actual_data_size *= sizeof(uint16_t);  // convert from count of uint16_t to bytes

        // Compress the data
        int compressedSize = LZ4_compress_default((const char*)chunk, dest, actual_data_size, LZ4_COMPRESSBOUND(actual_data_size));
        // printf("Thread %d compressed a chunk of size %zu to %d.\n", data->thread_id, actual_data_size, compressedSize);

        no_chunks_read+=1;
    }

    // free(dest);

    printf("Thread %d read total %d chunks.\n", data->thread_id, no_chunks_read);
    free(arg);
    return NULL;
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s filename num_threads\n", argv[0]);
        return 1;
    }

    char *filename = argv[1];
    int num_threads = atoi(argv[2]);

    size_t data_size;
    uint16_t *data = read_hdf5(filename, &data_size);
    printf("Data size from main thread: %zu\n", data_size);
    size_t chunk_size = 524288;  // adjust as needed

    DataGenerator *generator = create_generator(data, data_size, chunk_size);

    pthread_t *threads = (pthread_t*) malloc(sizeof(pthread_t) * num_threads);
    if(threads == NULL) {
        perror("Failed to allocate memory for threads");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_threads; i++) {
        ThreadData *data = (ThreadData*) malloc(sizeof(ThreadData));
        if(data == NULL) {
            perror("Failed to allocate memory for ThreadData");
            exit(EXIT_FAILURE);
        }
        data->generator = generator;
        data->thread_id = i;

        if (pthread_create(&threads[i], NULL, reader_thread, data) != 0) {
            perror("Failed to create thread");
            return 1;
        }
    }

    for (int i = 0; i < num_threads; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("Failed to join thread");
            return 1;
        }
    }

    destroy_generator(generator);
    free(threads);

    return 0;
}
