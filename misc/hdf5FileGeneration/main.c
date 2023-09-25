#include <stdio.h>
#include <stdlib.h>
#include <hdf5.h>
#include <time.h>

#define FILE        "/local/tomo_test.h5"
#define DATASETNAME "IntArray"
#define Z0 1500
#define Z1 2160
#define Z2 2560
#define RANK  3

int main (void)
{
    hid_t       file_id, dataset_id, space_id;  /* identifiers */
    hsize_t     dims[RANK] = {Z0, Z1, Z2};
    herr_t      status;
    size_t total_size = Z0 * Z1;
    total_size *= Z2;
    srand(time(NULL));

    uint16_t* data = malloc(total_size * sizeof(uint16_t));
    if (data == NULL) {
        printf("Failed to allocate memory!\n");
        return 1;
    }

    for (size_t i = 0; i < total_size; i++) {
        data[i] = 42 + rand() % 9;
    }

    file_id = H5Fcreate(FILE, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    space_id = H5Screate_simple(RANK, dims, NULL);
    dataset_id = H5Dcreate2(file_id, DATASETNAME, H5T_STD_U16LE, space_id,
                            H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    status = H5Dwrite(dataset_id, H5T_NATIVE_UINT16, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                      data);

    free(data);
    status = H5Dclose(dataset_id);
    status = H5Sclose(space_id);
    status = H5Fclose(file_id);

    return 0;
}
