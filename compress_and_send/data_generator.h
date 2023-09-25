#ifndef DATA_GENERATOR_H
#define DATA_GENERATOR_H

#include <stdlib.h>
#include <pthread.h>
#include <H5public.h>
#include <H5Fpublic.h>
#include <H5Dpublic.h>
#include <H5Spublic.h>
#include <H5Ppublic.h>
#include <H5Tpublic.h>
#include <pthread.h>
#include <stdint.h>
#include <hdf5.h>
#include <sched.h>
#include <numa.h>
#include <assert.h>
#include <numaif.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>


typedef struct {
    char *chunk;
    size_t size;
} DataChunk;

typedef struct DataGenerator {
    uint16_t *data;
    size_t dataSize;
    size_t chunk_size;
    size_t currentIndex;
    int numa_node;
    int current_call_number;
    int file_multiplier;
    int file_multiplier_counter;
    pthread_mutex_t mutex;
} DataGenerator;

DataGenerator* data_generator_init(uint16_t *data, size_t dataSize, size_t chunkSize, int node,int multiplier);
uint16_t* data_generator_next(DataGenerator *gen, size_t *outSize);
void data_generator_free(DataGenerator *gen);

uint16_t* read_hdf5(const char* filename, size_t *data_size, int numa_node,size_t *z_dim, size_t *y_dim, size_t *x_dim);
int get_memory_allocation_node(void *p);

#endif // DATA_GENERATOR_H
