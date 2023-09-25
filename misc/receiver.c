#include "/home/beams/MJAMIL/project/libzmq/include/zmq.h"
#include <stdio.h>
#include <stdlib.h>

int main (int argc, char *argv[])
{
    const char *bind_to;
    int message_count;
    size_t message_size;
    void *ctx;
    void *s;
    int rc;
    int i;
    zmq_msg_t msg;
    void *watch;
    unsigned long elapsed;
    double throughput;
    double megabits;
    size_t total_bytes = 0;  // variable to accumulate total bytes received


    if (argc != 4 && argc != 5) {
        printf ("usage: local_thr <bind-to> <message-size> <message-count> "
                "[<enable_curve>]\n");
        return 1;
    }
    bind_to = argv[1];
    message_size = atoi (argv[2]);
    message_count = atoi (argv[3]);


    ctx = zmq_init (1);
    if (!ctx) {
        printf ("error in zmq_init: %s\n", zmq_strerror (errno));
        return -1;
    }

    s = zmq_socket (ctx, ZMQ_PULL);
    if (!s) {
        printf ("error in zmq_socket: %s\n", zmq_strerror (errno));
        return -1;
    }
    rc = zmq_bind (s, bind_to);
    if (rc != 0) {
        printf ("error in zmq_bind: %s\n", zmq_strerror (errno));
        return -1;
    }

    rc = zmq_msg_init (&msg);
    if (rc != 0) {
        printf ("error in zmq_msg_init: %s\n", zmq_strerror (errno));
        return -1;
    }

    rc = zmq_recvmsg (s, &msg, 0);
    if (rc < 0) {
        printf ("error in zmq_recvmsg: %s\n", zmq_strerror (errno));
        return -1;
    }
    total_bytes += zmq_msg_size (&msg); // add the size of the first message

    watch = zmq_stopwatch_start ();

    for (i = 0; i != message_count - 1; i++) {
        rc = zmq_recvmsg (s, &msg, 0);
        if (rc < 0) {
            printf ("error in zmq_recvmsg: %s\n", zmq_strerror (errno));
            return -1;
        }
        total_bytes += zmq_msg_size (&msg); // accumulate the size of each message
    }

    elapsed = zmq_stopwatch_stop (watch);
    if (elapsed == 0)
        elapsed = 1;

    rc = zmq_msg_close (&msg);
    if (rc != 0) {
        printf ("error in zmq_msg_close: %s\n", zmq_strerror (errno));
        return -1;
    }
    message_size = total_bytes/message_count; // convert message size to bits
    throughput = ((double) message_count / (double) elapsed * 1000000);
    megabits = ((double) throughput * message_size * 8) / 1000000;

    printf ("message size: %d [B]\n", (int) message_size);
    printf ("message count: %d\n", (int) message_count);
    printf ("mean throughput: %d [msg/s]\n", (int) throughput);
    printf ("mean throughput: %.3f [Mb/s]\n", (double) megabits);
    printf ("total bytes received: %lu [B]\n", total_bytes); // print the total bytes received

    rc = zmq_close (s);
    if (rc != 0) {
        printf ("error in zmq_close: %s\n", zmq_strerror (errno));
        return -1;
    }

    rc = zmq_ctx_term (ctx);
    if (rc != 0) {
        printf ("error in zmq_ctx_term: %s\n", zmq_strerror (errno));
        return -1;
    }

    return 0;
}
