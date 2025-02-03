#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>

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

mqd_t mq_tx; // TX Message queue descriptor
mqd_t mq_rx; // RX Message queue descriptor

// 송신 함수
void *sender(void *arg) {
    while (1) {
    	pthread_mutex_lock(&tx_queue.lock);
    	if (isQueueEmpty(&tx_queue)) {
    		pthread_mutex_unlock(&tx_queue.lock); 
            continue;
    	}
    // 윈도우 내에서 패킷 생성 및 큐에 추가
    	printf("-------\n");
    	printqueue(&tx_queue);
        while (next_tx_seq < num_packets && (next_tx_seq - tx_queue.packets[tx_queue.front].seq_num < WINDOW_SIZE || isQueueEmpty(&tx_queue))) {
        	Packet packet;
    		packet.seq_num = next_tx_seq;
    		packet.state = TLOE_INIT;
    		if (enqueue(&tx_queue, packet)) {
    			printf("Creating packet with sequence number: %d\n", packet.seq_num);
    			next_tx_seq++;
    		} else break;
    	}
    }

	// 윈도우 내 모든 패킷 전송 (처음 전송 시에만 send_time 기록)
	for (int i = tx_queue.front; i != tx_queue.rear; i = (i + 1) % QUEUE_SIZE) {
		if (!(tx_queue.packets[i].state & ~TLOE_SENT)) { // if not sent
			tx_queue.packets[i].state = TLOE_SENT;
			tx_queue.packets[i].send_time = time(NULL); // 전송 시간 기록
			printf("Sending packet with sequence number: %d\n", tx_queue.packets[i].seq_num);
		}
	}

	// Wait for an acknowledgment or timeout
	pthread_cond_wait(&cond, &ack_queue.lock);

	// ACK 대기 및 처리, 타임아웃 확인
	//	recv_action = tloe_frame(cmd, ack, nack), timeout
    if (!isQueueEmpty(&tx_queue)) {
		if (rand() % 3 != 0) { // 2/3 확률로 ACK 성공
			int ack_num;
			printf("Enter ACK number: ");
			scanf("%d", &ack_num);

			// ACK 받은 만큼 큐에서 제거 및 윈도우 이동
			while (!isQueueEmpty(&tx_queue) && tx_queue.packets[tx_queue.front].seq_num <= ack_num) {
				if (tx_queue.packets[tx_queue.front].seq_num == ack_num) {
					tx_queue.packets[tx_queue.front].state |= TLOE_ACKED;
					printf("Received ACK for sequence number: %d\n", ack_num);
				}
				dequeue(&tx_queue);
			}
		} else { // 타임아웃
			printf("-- timeout\n");
			printqueue(&tx_queue);
			if(isTimeout(tx_queue.packets[tx_queue.front].send_time)){
				printf("Timeout occurred. Resending packets from sequence number: %d\n", tx_queue.packets[tx_queue.front].seq_num);
				for (int i = tx_queue.front; i != tx_queue.rear; i = (i + 1) % QUEUE_SIZE) {
					tx_queue.packets[i].state |= TLOE_TIMEOUT; // set timeout
				}
			}
		}
	}
	pthread_mutex_unlock(&tx_queue.lock);
}

void *receiver(void *arg) {
    char message[MAX_MESSAGE_SIZE];
    while (1) {
        // Receive frame information from the message queue
        if (mq_receive(mq_rx, message, MAX_MESSAGE_SIZE, NULL) == -1) {
            perror("[Receiver] mq_receive failed");
            continue;
        }

        printf("[Receiver] Received: %s\n", message);
        store_received_data(&recv_queue, message); // Store received data in receiver queue

        pthread_mutex_lock(&ack_queue.lock);
        if (ack_queue.base != ack_queue.next_seq_num) {
            printf("[Receiver] Acknowledging frame %d\n", ack_queue.base % MAX_SEQ_NUM);
            acknowledge_frame(&ack_queue, ack_queue.base % MAX_SEQ_NUM);
            ack_queue.base = (ack_queue.base + 1) % MAX_SEQ_NUM;

            // Signal the sender to wake up
            pthread_cond_signal(&cond);
        }
        pthread_mutex_unlock(&ack_queue.lock);
    }
    return NULL;
}

int main() {
    pthread_t sender_thread, receiver_thread, main_thread;
    struct mq_attr attr;

    srand(time(NULL));
    initQueue(&tx_queue);
    initQueue(&rx_queue);

    // Initialize message queue attributes
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MAX_MESSAGE_SIZE;
    attr.mq_curmsgs = 0;

    // Create message queue
    mq = mq_open(QUEUE_NAME, O_CREAT | O_RDWR, 0644, &attr);
    if (mq == (mqd_t)-1) {
        perror("mq_open failed");
        exit(EXIT_FAILURE);
    }

    // Initialize
    initialize_circular_queue(&ack_queue);
    initialize_receiver_queue(&recv_queue);

    // Create threads
    pthread_create(&sender_thread, NULL, sender, NULL);
    pthread_create(&receiver_thread, NULL, receiver, NULL);

    // Join threads
    pthread_join(sender_thread, NULL);
    pthread_join(receiver_thread, NULL);

    // Cleanup
    mq_close(mq);
    mq_unlink(QUEUE_NAME);

    return 0;
}
