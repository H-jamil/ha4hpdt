#include "data_generator.h"

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

DataGenerator* data_generator_init(uint16_t *data, size_t dataSize, size_t chunkSize, int node, int multiplier) {
    DataGenerator *gen = malloc(sizeof(DataGenerator));
    gen->data = data;
    gen->dataSize = dataSize;
    gen->chunk_size = chunkSize;
    gen->file_multiplier = multiplier;
    gen->currentIndex = 0;
    gen->file_multiplier_counter = 0;
    gen->numa_node=node;
    gen->current_call_number= 0;
    pthread_mutex_init(&gen->mutex, NULL);
    return gen;
}


uint16_t* data_generator_next(DataGenerator *gen, size_t *outSize) {
    pthread_mutex_lock(&gen->mutex);
    if (gen->currentIndex >= gen->dataSize) {
        // We have reached the end of the data
        gen->file_multiplier_counter++;
        if (gen->file_multiplier_counter >= gen->file_multiplier) {
            // We have reached the end of the file of the gen->file_multiplier
            pthread_mutex_unlock(&gen->mutex);
            return NULL;
        }
        gen->currentIndex = 0;
    }
    size_t size = gen->chunk_size;
    if (gen->currentIndex + size > gen->dataSize) {
        // Adjust the size of the last chunk if it would go past the end of the data
        size = gen->dataSize - gen->currentIndex;
    }

    uint16_t *chunk = gen->data + gen->currentIndex / sizeof(uint16_t);
    gen->currentIndex += size;

    pthread_mutex_unlock(&gen->mutex);

    *outSize = size;
    gen->current_call_number++;

    return chunk;
}



void data_generator_free(DataGenerator *gen) {
    pthread_mutex_destroy(&gen->mutex);
    free(gen);
}



uint16_t* read_hdf5(const char* filename,size_t* d_size,int node,size_t *z_dim, size_t *y_dim, size_t *x_dim) {
    // read data from hdf5 file
    printf("Reading data from file: %s\n", filename);
    clock_t start_time = clock();
    // create array to store data
    hid_t file_id, dataset_id, dataspace_id;
    hsize_t dims[3];
    // Open the HDF5 file
    printf("Opening file: %s\n", filename);
    file_id = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
    // Open the dataset
    // printf("Opening dataset: exchange/data\n");
    // dataset_id = H5Dopen2(file_id, "exchange/data", H5P_DEFAULT);
    printf("Opening dataset: IntArray\n");
    dataset_id = H5Dopen2(file_id, "IntArray", H5P_DEFAULT);
    // Get the dataspace
    dataspace_id = H5Dget_space(dataset_id);
    // Get the dimensions of the dataset
    H5Sget_simple_extent_dims(dataspace_id, dims, NULL);
    *d_size = dims[0] * dims[1] * dims[2] * sizeof(uint16_t); // Save the size to the provided variable
    *z_dim = dims[0];
    *y_dim = dims[1];
    *x_dim = dims[2];
    // Allocate memory for the data
    printf("Allocating memory for data on NUMA node %d...\n", node);
    uint16_t *data = (uint16_t*) numa_alloc_onnode(dims[0] * dims[1] * dims[2] * sizeof(uint16_t), node);
     // Read the data from the dataset
    printf("Reading data...\n");
    H5Dread(dataset_id, H5T_NATIVE_UINT16, H5S_ALL, H5S_ALL, H5P_DEFAULT, data);
    // Close the dataspace
    H5Sclose(dataspace_id);
    // Close the dataset
    H5Dclose(dataset_id);
    // Close the file
    H5Fclose(file_id);
    clock_t elapsed_read = clock() - start_time;
    printf("Read time: %ld milliseconds\n", (1000 * elapsed_read) / CLOCKS_PER_SEC);
    printf("Size of the data dimensions (in bytes): %llu * %llu * %llu \n", dims[0], dims[1], dims[2]);
    get_memory_allocation_node(data);
    return data;
}

