#ifndef __CIRCULAR_QUEUE__
#define __CIRCULAR_QUEUE__
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#define QUEUE_SIZE 5  // >= WINDOW_SIZE + 1

typedef struct {
    void* data[QUEUE_SIZE];
    int front;
    int rear;

    pthread_mutex_t lock; // Mutex for thread-safe operations
    pthread_cond_t cond; // Condition variable for signaling
} CircularQueue;

typedef struct {
    CircularQueue *q;
    int next;
} CircularQueueIter;

void init_queue(CircularQueue *queue);
bool is_queue_empty(CircularQueue *queue);
bool is_queue_full(CircularQueue *queue);
bool enqueue(CircularQueue *queue, void *data);
void* dequeue(CircularQueue *queue);
void* getfront(CircularQueue *q);
void printqueue(CircularQueue *queue);
CircularQueueIter *queue_iter(CircularQueue *q);

#endif // __CIRCULAR_QUEUE_H_
