#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <H5public.h>
#include <H5Fpublic.h>
#include <H5Dpublic.h>
#include <H5Spublic.h>
#include <H5Ppublic.h>
#include <H5Tpublic.h>
#include <string.h>

char* read_hdf5(const char* filename) {
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
    char *data = (char*) malloc(dims[0] * dims[1] * dims[2] * sizeof(uint16_t));

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

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s filename num_threads\n", argv[0]);
        return 1;
    }

    char *filename = argv[1];
    int num_threads = atoi(argv[2]);

    // insert the read_hdf5 function here
    char *data = read_hdf5(filename);
    printf("data[16588801000] = %d\n", data[16588801000]);
    return 0;
}
