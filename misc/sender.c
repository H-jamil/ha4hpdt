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

volatile int is_thread_running = 1; //for monitoring thread

int NUM_THREADS=1;


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
    printf("allocated data is on node %d\n",get_memory_allocation_node(data));
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

typedef struct {
    char* data;
    int len;
} Chunk;

typedef struct {
    Chunk* chunks[1000];
    int start;
    int end;
    int is_finished;
    int num_chunks;
    pthread_mutex_t mutex;
    pthread_cond_t can_produce;
    pthread_cond_t can_consume;
} Queue;

Queue queue = {
    .start = 0,
    .end = 0,
    .is_finished = 0,
    .num_chunks = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .can_produce = PTHREAD_COND_INITIALIZER,
    .can_consume = PTHREAD_COND_INITIALIZER
};

void enqueue(Queue* q, char* data, int len) {
    pthread_mutex_lock(&q->mutex);
    while ((q->end + 1) % NUM_THREADS == q->start) {
        pthread_cond_wait(&q->can_produce, &q->mutex);
    }
    // printf("Enqueuing chunk %d\n", q->end);
    Chunk* chunk = malloc(sizeof(Chunk));
    chunk->data = data;
    chunk->len = len;
    q->chunks[q->end] = chunk;
    q->end = (q->end + 1) % NUM_THREADS;
    q->num_chunks+=1;
    pthread_cond_signal(&q->can_consume);
    pthread_mutex_unlock(&q->mutex);
}

Chunk* dequeue(Queue* q) {
    pthread_mutex_lock(&q->mutex);
    while (q->end == q->start && !q->is_finished) {
        pthread_cond_wait(&q->can_consume, &q->mutex);
    }
    if (q->end == q->start) {
        pthread_mutex_unlock(&q->mutex);
        return NULL;
    }
    // printf("Dequeuing chunk %d\n", q->start);
    Chunk* chunk = q->chunks[q->start];
    q->start = (q->start + 1) % NUM_THREADS;
    q->num_chunks-=1;
    pthread_cond_signal(&q->can_produce);
    pthread_mutex_unlock(&q->mutex);
    return chunk;
}

void* queue_monitor_thread(void *arg){
    // int count =0;
    while(is_thread_running ) {
        usleep(10000);  // sleep for 10 ms
        pthread_mutex_lock(&queue.mutex);
        time_t t = time(NULL);
        printf("Time: %s Size: %d\n", ctime(&t), queue.num_chunks);
        pthread_mutex_unlock(&queue.mutex);
        // count++;
    }
    printf("Monitor thread exiting...\n");
    pthread_exit(NULL);
}


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
    while ((chunk = next_chunk(generator)) != NULL) {
        // Print chunk size
        char* dest = malloc(dest_size);
        if (dest == NULL) {
            printf("Failed to allocate memory for compression.\n");
            free(arg);
            pthread_exit(NULL);
        }
        size_t actual_data_size = generator->chunk_size;
        if (generator->current_index > generator->size - generator->chunk_size) {
            // actual_data_size = generator->size % generator->chunk_size;
            break;
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
        enqueue(&queue, dest, compressedSize);
    }
    printf("Thread %d read total %d chunks.\n", data->thread_id,no_chunks_read);
    queue.is_finished = 1;
    free(arg);
    pthread_exit(NULL);
}

typedef struct {
    char *ip_port;
    int thread_id;
    int numa_node;
} send_thread_args;

void* dequeue_and_send(void* arg) {
    send_thread_args *params = (send_thread_args*) arg;
    struct bitmask *nodemask = numa_allocate_nodemask();
    numa_bitmask_clearall(nodemask);
    numa_bitmask_setbit(nodemask, params->numa_node);
    numa_bind(nodemask);
    numa_free_nodemask(nodemask);
    int no_chunks_send=0;
    char* r_ip_port = params->ip_port;
    // printf("Thread %d connecting to %s\n", params->thread_id, r_ip_port);
    void *context = zmq_init (1);
    void *socket = zmq_socket(context, ZMQ_PUSH);
    int rc = zmq_connect(socket, r_ip_port);
    assert(rc == 0);
    // printf("connected\n");
    zmq_msg_t msg;
    while (queue.num_chunks > 0 || !queue.is_finished) {
        Chunk* chunk = dequeue(&queue);
        if (chunk == NULL) {
            pthread_exit(NULL);
        }
        // printf("Thread %d sending chunk of size %d\n", params->thread_id, chunk->len);
        // Initialize a zmq_msg_t with the size of the chunk
        rc = zmq_msg_init_size(&msg, chunk->len);
        assert(rc == 0);
        // Copy the data into the zmq_msg_t
        memcpy(zmq_msg_data(&msg), chunk->data, chunk->len);

        // Send the data
        int bytes_sent = zmq_sendmsg(socket, &msg, 0);
        assert(bytes_sent == chunk->len);

        free(chunk->data);
        free(chunk);
        zmq_msg_close(&msg);
        no_chunks_send+=1;
    }
    zmq_close(socket);
    zmq_ctx_term(context);
    // free(arg->ip_port);
    printf("Send thread %d exiting with send count %d...\n", params->thread_id,no_chunks_send);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 9) {
        printf("Usage: %s <filename> <#compressionthreads> <#sendthreads> <ip-address> <port> <memory_node> <thread_node_c> <thread_node_s>\n", argv[0]);
        return 1;
    }

    char *filename = argv[1];
    int num_threads = atoi(argv[2]);
    NUM_THREADS = num_threads;
    int memory_node = atoi(argv[6]);
    int thread_node = atoi(argv[7]);
    int num_send_threads = atoi(argv[3]);
    int send_thread_node = atoi(argv[8]);
    size_t data_size;
    // insert the read_hdf5 function here
    char *data = read_hdf5(filename,&data_size,memory_node);

    size_t chunk_size = 1658880;  // adjust as needed

    DataGenerator *generator = create_generator(data, data_size, chunk_size);

    pthread_t *threads = malloc(sizeof(pthread_t) * num_threads);
    pthread_t *send_threads = malloc(sizeof(pthread_t) * num_send_threads);
    pthread_t queue_monitor;
    // pthread_create(&queue_monitor, NULL, queue_monitor_thread, NULL);
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);  // start timing
    for (int i = 0; i < num_threads; i++) {
        ThreadData *data = malloc(sizeof(ThreadData));
        data->generator = generator;
        data->thread_id = i;
        data->numa_node = thread_node;

        if (pthread_create(&threads[i], NULL, reader_thread, data) != 0) {
            printf("Failed to create compress thread %d.\n", i);
            return 1;
        }
    }
    for (int i = 0; i < num_send_threads; i++) {
        send_thread_args *data = malloc(sizeof(send_thread_args));
        char *remote_ip_port=(char*)malloc(50);
        int port = atoi(argv[5]) + i;
        char *port_str=(char*)malloc(6);
        sprintf(port_str, "%d", port);
        snprintf(remote_ip_port, 50, "tcp://%s:%s", argv[4], port_str);
        data->ip_port = remote_ip_port;
        data->thread_id = i;
        data->numa_node = send_thread_node;
        if (pthread_create(&send_threads[i], NULL, dequeue_and_send, data) != 0) {
            printf("Failed to create send thread %d.\n", i);
            return 1;
        }
        // printf("Created send thread with ip port %s\n", remote_ip_port);
    }
    for (int i = 0; i < num_threads; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            printf("Failed to join thread %d.\n", i);
            return 1;
        }
    }
    for (int i = 0; i < num_send_threads; i++) {
        if (pthread_join(send_threads[i], NULL) != 0) {
            printf("Failed to join thread %d.\n", i);
            return 1;
        }
    }
    gettimeofday(&end_time, NULL);  // end timing

    long elapsed_time = ((end_time.tv_sec - start_time.tv_sec) * 1000) +
                        ((end_time.tv_usec - start_time.tv_usec) / 1000);
    is_thread_running = 0;
    // pthread_cancel(queue_monitor);
    // pthread_join(queue_monitor, NULL);
    double sending_throughput = (data_size / elapsed_time);
    double sending_throuhgput_gbps =sending_throughput * 8 / 0.001 / 1e9;
    printf("Elapsed time: %ld milliseconds with %.6f Gbps\n", elapsed_time, sending_throuhgput_gbps);

    destroy_generator(generator);

    free(threads);
    free(send_threads);

    return 0;
}
