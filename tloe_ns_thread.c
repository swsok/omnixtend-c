#include "tloe_mq.h"
#include "tloe_fabric.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <mqueue.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define MAX_QUEUE_NAME 64

static pthread_t tloe_ns_thread;
static volatile int is_done = 1;
static TloeMQ *ep_port_a;
static TloeMQ *ep_port_b;

// Add drop counters
static volatile int drop_count_a_to_b = 0;
static volatile int drop_count_b_to_a = 0;

static int thread_init(const char *queue_name)
{
    char port_a_queue[MAX_QUEUE_NAME];
    char port_b_queue[MAX_QUEUE_NAME];

    // Construct queue names using the provided base name
    snprintf(port_a_queue, sizeof(port_a_queue), "%s-a", queue_name);
    snprintf(port_b_queue, sizeof(port_b_queue), "%s-b", queue_name);

    // Initialize message queues for both endpoints
    ep_port_a = tloe_mq_open(port_a_queue, "-slave", TLOE_FABRIC_QFLAGS_CREATE);
    if (ep_port_a == NULL) {
        printf("Failed to open port A message queue\n");
        return -1;
    }

    ep_port_b =
        tloe_mq_open(port_b_queue, "-master", TLOE_FABRIC_QFLAGS_CREATE);
    if (ep_port_b == NULL) {
        tloe_mq_close(ep_port_a);
        printf("Failed to open port B message queue\n");
        return -1;
    }

    return 0;
}

static void thread_close(void)
{
    // Cleanup message queues
    if (ep_port_a != NULL) {
        tloe_mq_close(ep_port_a);
        ep_port_a = NULL;
    }
    if (ep_port_b != NULL) {
        tloe_mq_close(ep_port_b);
        ep_port_b = NULL;
    }
}

void tloe_ns_set_drop_count_a_to_b(int count)
{
    drop_count_a_to_b = count;
    printf("Will drop next %d requests from Port A to Port B\n", count);
}

void tloe_ns_set_drop_count_b_to_a(int count)
{
    drop_count_b_to_a = count;
    printf("Will drop next %d requests from Port B to Port A\n", count);
}

void tloe_ns_set_drop_count_bidirectional(int count)
{
    drop_count_a_to_b = count;
    drop_count_b_to_a = count;
    printf("Will drop next %d requests from Port A to Port B and B to A\n", count);
}

static void *thread_fn(void *arg)
{
    char buffer[BUFFER_SIZE];
    ssize_t recv_size_port_a;
    ssize_t recv_size_port_b;
    const char *queue_name = (const char *)arg;

    if (thread_init(queue_name) < 0) {
        printf("tloe_ns thread initialization failed\n");
        return NULL;
    }

    while (!is_done) {
        // Relay messages from port A to port B
        recv_size_port_a = tloe_mq_recv(ep_port_a, buffer, BUFFER_SIZE);
        if (recv_size_port_a > 0) {
            if (drop_count_a_to_b > 0) {
                // Drop the message and decrease counter
                drop_count_a_to_b--;
                printf("Dropped message from A to B (%d remaining)\n", drop_count_a_to_b);
            } else {
                // Normal relay
                if (tloe_mq_send(ep_port_b, buffer, recv_size_port_a) < 0) {
                    fprintf(stderr, "Dropped message of size %zd\n",
                            recv_size_port_a);
                }
            }
        }

        // Relay messages from port B to port A
        recv_size_port_b = tloe_mq_recv(ep_port_b, buffer, BUFFER_SIZE);
        if (recv_size_port_b > 0) {
            if (drop_count_b_to_a > 0) {
                // Drop the message and decrease counter
                drop_count_b_to_a--;
                printf("Dropped message from B to A (%d remaining)\n", drop_count_b_to_a);
            } else {
                // Normal relay
                if (tloe_mq_send(ep_port_a, buffer, recv_size_port_b) < 0) {
                    fprintf(stderr, "Dropped message of size %zd\n",
                            recv_size_port_b);
                }
            }
        }

        // If no messages are received, sleep for 1ms
        if (recv_size_port_a + recv_size_port_b < 0)
            usleep(1000);
    }

    thread_close();
    
    return NULL;
}

void tloe_ns_thread_start(const char *queue_name)
{
    if (!is_done) {
        printf("tloe_ns thread is already running\n");
        return;
    }

    is_done = 0;
    if (pthread_create(&tloe_ns_thread, NULL, thread_fn,
                       (void *)queue_name) != 0) {
        perror("Failed to create tloe_ns thread");
        return;
    }
    printf("tloe_ns thread started\n");
}

void tloe_ns_thread_stop(void)
{
    if (is_done) {
        printf("tloe_ns thread is not running\n");
        return;
    }

    is_done = 1;
    pthread_join(tloe_ns_thread, NULL);
    printf("tloe_ns thread stopped\n");
}

static int flush_message_queue(mqd_t mq)
{
    char buffer[BUFFER_SIZE];
    struct mq_attr attr;
    ssize_t recv_size;

    // Get current attributes to check message count
    if (mq_getattr(mq, &attr) == -1) {
        perror("mq_getattr failed");
        return -1;
    }

    // Temporarily set to non-blocking mode
    struct mq_attr old_attr = attr;
    attr.mq_flags |= O_NONBLOCK;
    if (mq_setattr(mq, &attr, NULL) == -1) {
        perror("mq_setattr failed");
        return -1;
    }

    // Read all messages until queue is empty
    while ((recv_size = mq_receive(mq, buffer, BUFFER_SIZE, NULL)) >= 0) {
        printf("Flushed message of size %zd\n", recv_size);
    }

    // Restore original flags
    if (mq_setattr(mq, &old_attr, NULL) == -1) {
        perror("mq_setattr (restore) failed");
        return -1;
    }

    return 0;
}

void tloe_ns_flush_ports(void)
{
    if (is_done) {
        printf("tloe_ns thread is not running, cannot flush ports\n");
        return;
    }

    printf("Flushing port A...");
    if (flush_message_queue(ep_port_a->mq_tx) == 0 &&
        flush_message_queue(ep_port_a->mq_rx) == 0) {
        printf("Port A flushed successfully\n");
    }

    printf("Flushing port B...");
    if (flush_message_queue(ep_port_b->mq_tx) == 0 &&
        flush_message_queue(ep_port_b->mq_rx) == 0) {
        printf("Port B flushed successfully\n");
    }
}

int tloe_ns_thread_is_done(void)
{
	return is_done;
}