#include "sender.h"

void* dequeue_and_send(void* args) {
    send_thread_args *params = (send_thread_args*) args;
    struct bitmask *nodemask = numa_allocate_nodemask();
    numa_bitmask_clearall(nodemask);
    numa_bitmask_setbit(nodemask, params->numa_node);
    numa_bind(nodemask);
    numa_free_nodemask(nodemask);
    int num_chunks_send= 0;
    int rc;
    zmq_msg_t msg;
    void *s;
    void *ctx;
    ctx = zmq_init (1);
    if (!ctx) {
        printf ("Thread id %d error in zmq_init: %s\n", params->thread_id, zmq_strerror (errno));
        return NULL;
    }
    s = zmq_socket (ctx, ZMQ_PUSH);
    if (!s) {
        printf ("Thread id %d error in zmq_socket: %s\n", params->thread_id,zmq_strerror (errno));
        return NULL;
    }
    rc = zmq_connect (s, params->connect_to);

    if (rc != 0) {
        printf (" Thread id %d error in zmq_connect: %s\n",params->thread_id,zmq_strerror (errno));
        return NULL;
    }
    printf ("thread %d connected to %s\n", params->thread_id,params->connect_to);
    while (1) {
        Chunk *chunk =(Chunk*) queue_pop(params->queue);
        if (chunk == NULL) {
            break;
        }
        rc = zmq_msg_init_size(&msg, chunk->len);
        assert(rc == 0);
        // Copy the data into the zmq_msg_t
        memcpy(zmq_msg_data(&msg), chunk->data, chunk->len);

        // Send the data
        int bytes_sent = zmq_msg_send(&msg, s, 0);
        assert(bytes_sent == chunk->len);
        free(chunk->data);
        free(chunk);
        rc = zmq_msg_close (&msg);
        if (rc != 0) {
            printf ("Thread id %d error in zmq_msg_close: %s\n", params->thread_id,zmq_strerror (errno));
            return NULL;
        }
        num_chunks_send++;
    }
    const char *termMsg = "TERMINATE";
    zmq_msg_t msg_term;
    zmq_msg_init_size(&msg_term, strlen(termMsg) + 1);
    memcpy(zmq_msg_data(&msg_term), termMsg, strlen(termMsg) + 1);
    zmq_msg_send(&msg_term, s, 0);
    zmq_msg_close(&msg_term);
    zmq_close(s);
    zmq_ctx_destroy(ctx);
    printf("Send thread %d exiting with send count %d...\n", params->thread_id,num_chunks_send);
    free(params);
    pthread_exit(NULL);
}
