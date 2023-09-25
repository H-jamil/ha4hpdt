
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "data_generator.h"
#include "reader.h"
#include "queue.h"
#include "sender.h"
size_t total_bytes_send_size=0;
pthread_mutex_t total_bytes_send_size_lock;
int queue_null_flag=0;
pthread_mutex_t queue_null_flag_lock;

int main(int argc, char *argv[]) {
    if (argc != 11) {        //1              2                  3                   4               5                   6         7       8             9            10
        printf("Usage: %s <filename> <#compressionthreads> <#sendingthreads> <file_memory_node> <compression_node> <sending_node> <ip> <port> <queue_memory_node><file-multiplier>\n", argv[0]);
        return 1;
    }

    char *filename = argv[1];
    int num_compression_threads = atoi(argv[2]);
    int num_sending_threads = atoi(argv[3]);
    int memory_node = atoi(argv[4]);
    int compression_thread_node = atoi(argv[5]);
    int sending_thread_node = atoi(argv[6]);
    int queue_memory_node = atoi(argv[9]);
    size_t* data_size = malloc(sizeof(size_t));
    size_t* z_dim=malloc(sizeof(size_t));
    size_t* y_dim=malloc(sizeof(size_t));
    size_t* x_dim=malloc(sizeof(size_t));
    uint16_t* data = read_hdf5(filename,data_size,memory_node,z_dim,y_dim,x_dim);
    if (data == NULL) {
        printf("Error reading data from hdf5 file\n");
        return 1;
    }
    int file_multiplier = atoi(argv[10]);
    size_t actual_data_size = (*data_size) * file_multiplier;
    printf("Data size: %zu\n", *data_size);
    printf("Data size after file multiplier: %zu\n", actual_data_size);
    // DataGenerator *gen = data_generator_init(data, data_size, CHUNK_SIZE, memory_node);
    printf("Chunk size: %zu\n", *y_dim * *x_dim * sizeof(uint16_t));
    DataGenerator *generator = data_generator_init(data, *data_size, *y_dim * *x_dim * sizeof(uint16_t),memory_node,file_multiplier);
    if (generator == NULL) {
        printf("Error initializing data generator\n");
        return 1;
    }
    Queue *queue=queue_create();

    struct timeval start_time, end_time;
    pthread_t *compression_threads = malloc(sizeof(pthread_t) * num_compression_threads);
    pthread_t *sending_threads = malloc(sizeof(pthread_t) * num_sending_threads);
    pthread_mutex_init(&total_bytes_send_size_lock,NULL);
    pthread_mutex_init(&queue_null_flag_lock,NULL);
    gettimeofday(&start_time, NULL);  // start timing
    for (int i = 0; i < num_compression_threads; i++) {
        prodThreadData *c_t_data = malloc(sizeof(prodThreadData));
        c_t_data->generator = generator;
        c_t_data->thread_id = i;
        c_t_data->queue = queue;
        c_t_data->queue_memory_location= queue_memory_node;
        c_t_data->num_consumer_threads = num_sending_threads;
        // c_t_data->numa_node = compression_thread_node;
        if ( i % 2 == 0) {
            c_t_data->numa_node = compression_thread_node;
        }
        else{
            c_t_data->numa_node = compression_thread_node^1;
        }
        if (pthread_create(&compression_threads[i], NULL, reader_thread, c_t_data) != 0) {
            printf("Failed to create compress thread %d.\n", i);
            return 1;
        }
    }
    for (int i = 0; i < num_sending_threads; i++) {
        send_thread_args *s_t_data = malloc(sizeof(send_thread_args));
        char *remote_ip_port=(char*)malloc(50);
        int port = atoi(argv[8]) + i;
        char *port_str=(char*)malloc(6);
        sprintf(port_str, "%d", port);
        snprintf(remote_ip_port, 50, "tcp://%s:%s", argv[7], port_str);
        s_t_data->connect_to = remote_ip_port;
        s_t_data->queue = queue;
        s_t_data->thread_id = i;
        s_t_data->numa_node = sending_thread_node;
        if (pthread_create(&sending_threads[i], NULL, dequeue_and_send, s_t_data) != 0) {
            printf("Failed to create sending thread %d.\n", i);
            return 1;
        }
    }
    for (int i = 0; i < num_compression_threads; i++) {
        if (pthread_join(compression_threads[i], NULL) != 0) {
            printf("Failed to join thread %d.\n", i);
            return 1;
        }
    }
    for (int i = 0; i < num_sending_threads; i++) {
        if (pthread_join(sending_threads[i], NULL) != 0) {
            printf("Failed to join thread %d.\n", i);
            return 1;
        }
    }
    gettimeofday(&end_time, NULL);  // end timing

    long elapsed_time = ((end_time.tv_sec - start_time.tv_sec) * 1000) +
                        ((end_time.tv_usec - start_time.tv_usec) / 1000);

    printf("Elapsed time in generating and compressing: %ld milliseconds\n", elapsed_time);
    printf("generator->call_number: %d\n", generator->current_call_number);
    printf("Queue size after compression+send finshed: %d\n", queue_size(queue));
    double sending_throughput = (total_bytes_send_size / elapsed_time);
    double sending_throuhgput_gbps =sending_throughput * 8 / 0.001 / 1e9;
    printf("Elapsed time: %ld milliseconds with %.6f Gbps\n", elapsed_time, sending_throuhgput_gbps);
    printf("Total bytes send: %zu\n",total_bytes_send_size);
    data_generator_free(generator);
    int residue_queue=0;
    while(queue_size(queue)!=0){
        Chunk *chunk =(Chunk*)queue_pop(queue);
        residue_queue++;
        // printf("residue queue %d Chunk->len: %d\n",residue_queue,chunk->len);
        numa_free(chunk->data,chunk->len);
    }
    printf("Queue size after program finshed: %d\n", queue_size(queue));
    queue_destroy(queue);
    free(compression_threads);
    free(sending_threads);
    return 0;
}
