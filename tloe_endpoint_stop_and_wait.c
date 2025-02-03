#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "tloe_ether.h"

#define WINDOW_SIZE 	4
#define QUEUE_SIZE 	(WINDOW_SIZE + 1)
#define MAX_SEQ_NUM 	((1<<10)-1)
#define TIMEOUT_SEC 	2 // 타임아웃 시간 (초)
		      
#define TLOE_INIT 	(1<<0)
#define TLOE_SENT 	(1<<1)
#define TLOE_ACKED 	(1<<2)
#define TLOE_TIMEOUT 	(1<<3)

// 패킷 구조체 (이전 코드와 동일)
typedef struct {
    int seq_num;
    int state;
    time_t send_time; // 패킷 전송 시간 기록
    char data[20];
} Packet;

// 순환 큐 구조체 (이전 코드와 동일)
typedef struct {
    Packet packets[QUEUE_SIZE];
    int front;
    int rear;

    pthread_mutex_t lock; // Mutex for thread-safe operations
    pthread_cond_t cond; // Condition variable for signaling
} CircularQueue;

// 순환 큐 초기화
void initQueue(CircularQueue *queue) {
    queue->front = 0;
    queue->rear = 0;

    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

// 큐가 비어 있는지 확인
bool isQueueEmpty(CircularQueue *queue) {
    return queue->front == queue->rear;
}

// 큐가 가득 차 있는지 확인
bool isQueueFull(CircularQueue *queue) {
    return (queue->rear + 1) % QUEUE_SIZE == queue->front;
}

// 큐에 패킷 추가
bool enqueue(CircularQueue *queue, Packet packet) {
    if (isQueueFull(queue)) {
        return false; // 큐가 가득 참
    }
    queue->packets[queue->rear] = packet;
    queue->rear = (queue->rear + 1) % QUEUE_SIZE;
    return true;
}

// 큐에서 패킷 제거 (실제 제거는 아니고 front만 이동)
void dequeue(CircularQueue *queue) {
    if (!isQueueEmpty(queue)) {
        queue->front = (queue->front + 1) % QUEUE_SIZE;
    }
}

void printqueue(CircularQueue *queue) {
	int i;
       
	printf("front: %d, rear: %d\n", queue->front, queue->rear);
	for (i=queue->front; i != queue->rear; i = (i+1) % QUEUE_SIZE) {
		printf("queue->packets[%d].seq_num: %d, state: %x, send_time: %ld\n", i, 
				queue->packets[i].seq_num,
				queue->packets[i].state,
				queue->packets[i].send_time);
	}
}

// 타임아웃 발생 여부 확인 함수
bool isTimeout(time_t send_time) {
    return difftime(time(NULL), send_time) >= TIMEOUT_SEC;
}

CircularQueue tx_queue;
CircularQueue rx_queue;

int next_tx_seq = 0;
int next_rx_seq = 0;
int acked_seq = MAX_SEQ_NUM;

// 송신 함수
void *sender(void *arg) {
    TloeEther *ether = (TloeEther *)arg;
    char tloe_frame[TLOE_ETHER_MSG_SIZE];
    int tx_seq_num = 0;

    while (1) {
        int size;

    	pthread_mutex_lock(&tx_queue.lock);
        size = sprintf(tloe_frame,"Frmae-%d", tx_seq_num++);
        tloe_ether_send(ether, tloe_frame, TLOE_ETHER_MSG_SIZE);
        pthread_cond_wait(&tx_queue.cond, &tx_queue.lock);
        pthread_mutex_unlock(&tx_queue.lock);
    }
}

void *receiver(void *arg) {
    TloeEther *ether = (TloeEther *)arg;
    char tloe_frame[TLOE_ETHER_MSG_SIZE];

    while (1) {
        int size;

        pthread_mutex_unlock(&rx_queue.lock);
        size = tloe_ether_recv(ether, tloe_frame);
        printf("RX: size: %d, frame: %s\n", size, tloe_frame);
        pthread_cond_signal(&tx_queue.cond);
        pthread_mutex_unlock(&rx_queue.lock);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    pthread_t sender_thread, receiver_thread, main_thread;
    TloeEther *ether;
    int master_slave = 1;

    if (argc < 3) {
        printf("Usage: tloe_endpoint queue_name master[1]/slave[0]\n");
        exit(0);
    }


    if (argv[2][0] == '1')
        ether = tloe_ether_open(argv[1], 1);
    else
        ether = tloe_ether_open(argv[1], 0);

    initQueue(&tx_queue);
    initQueue(&rx_queue);

    // Create threads
    pthread_create(&sender_thread, NULL, sender, (void *) ether);
    pthread_create(&receiver_thread, NULL, receiver, (void *) ether);

    // Join threads
    pthread_join(sender_thread, NULL);
    pthread_join(receiver_thread, NULL);

    // Cleanup
    tloe_ether_close(ether);

    return 0;
}
