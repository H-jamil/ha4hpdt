#include "decompression.h"
#include "receiver.h"
#include "queue.h"
#include <pthread.h>
#include <stdint.h>
#include <hdf5.h>
#include "lz4.h"
#include <sched.h>
#include <numa.h>
#include <assert.h>
#include <numaif.h>
#include <unistd.h>
#include <sys/time.h>

extern size_t total_decompressed_size;
extern pthread_mutex_t total_decompressed_size_lock;

void* dequeue_and_decompress(void* args){
  decompression_thread_args *params = (decompression_thread_args*) args;
  // struct bitmask *nodemask = numa_allocate_nodemask();
  // numa_bitmask_clearall(nodemask);
  // numa_bitmask_setbit(nodemask, params->numa_node);
  // numa_bind(nodemask);
  // numa_free_nodemask(nodemask);
  int num_chunks_decompressed= 0;
  size_t decompressed_size = 0;
  int decompressedSize= 0;
  printf("decompression thread %d started\n", params->thread_id);
  char* decompressedData = malloc(params->chunk_size);
  if(decompressedData == NULL){
    printf("Error allocating memory for decompression\n");
    pthread_exit(NULL);
  }
  while(1){
    Message *msg = queue_pop(params->queue);
    if(msg == NULL){
      break;
    }
    // printf("decompression thread %d got message with size %ld\n", params->thread_id, msg->size);
    if (msg->data == NULL) {
      printf("decompression thread %d got NULL data\n", params->thread_id);
      continue;
    }
    decompressedSize = LZ4_decompress_safe(msg->data, decompressedData, msg->size, params->chunk_size);
    // printf("Thread %d de-compressed a chunk of size %zu to %d.\n", params->thread_id, msg->size, decompressedSize);
    assert(decompressedSize >= 0);
    decompressed_size += decompressedSize;
    num_chunks_decompressed++;
    // numa_free(msg->data,msg->size);
  }
  pthread_mutex_lock(&total_decompressed_size_lock);
  total_decompressed_size += decompressed_size;
  pthread_mutex_unlock(&total_decompressed_size_lock);
  printf("decompression thread %d finished with %d chunks\n", params->thread_id, num_chunks_decompressed);
  pthread_exit(NULL);
}
