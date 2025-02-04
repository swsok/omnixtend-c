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

typedef struct {
    pthread_mutex_t lock; // Mutex for thread-safe operations
    pthread_cond_t cond; // Condition variable for signaling
                         //
    int front;
    int rear;
    int size;

    void* data[0];
} CircularQueue;

typedef struct {
    CircularQueue *q;
    int next;
} CircularQueueIter;

void init_queue(CircularQueue *queue, int size);
void delete_queue(CircularQueue *q);
bool is_queue_empty(CircularQueue *queue);
bool is_queue_full(CircularQueue *queue);
bool enqueue(CircularQueue *queue, void *data);
void* dequeue(CircularQueue *queue);
void* getfront(CircularQueue *q);
void printqueue(CircularQueue *queue);
CircularQueue *create_queue(int size); 
CircularQueueIter *queue_iter(CircularQueue *q);

#endif // __CIRCULAR_QUEUE_H_

