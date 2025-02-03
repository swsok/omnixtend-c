#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include "tloe_ether.h"
#include "tloe_frame.h"
#include "util/circular_queue.h"

#define WINDOW_SIZE 	4
#define MAX_SEQ_NUM 	((1<<10)-1)
#define TIMEOUT_SEC 	2 // 타임아웃 시간 (초)
		      
#define TLOE_INIT 	(1<<0)
#define TLOE_SENT 	(1<<1)
#define TLOE_RESENT 	(1<<2)
#define TLOE_ACKED 	(1<<3)
#define TLOE_TIMEOUT 	(1<<4)

// 패킷 구조체 (이전 코드와 동일)
typedef struct {
    int seq_num;
    int state;
    time_t send_time; // 패킷 전송 시간 기록
    TloeFrame tloe_frame;
    void (*print)(void);
} RetransmitBufferElement;


bool isTimeout(time_t send_time) {
    return difftime(time(NULL), send_time) >= TIMEOUT_SEC;
}


CircularQueue retransmit_buffer;
CircularQueue rx_buffer;

int next_tx_seq = 0;
int next_rx_seq = 0;
int acked_seq = MAX_SEQ_NUM;
int acked = 0;

int retransmit(TloeEther *ether, int nack_seq_num) {
    int i, n;
    TloeFrame frame;
    // update ACKD_SEQ ????
    // retransmit 
    n = 0;
    for (i=retransmit_buffer.front; i != retransmit_buffer.rear; i++){
        RetransmitBufferElement *element = (RetransmitBufferElement *) retransmit_buffer.data[i];
        if (element->seq_num > frame.seq_num_ack) {
            frame.seq_num = element->seq_num;
            frame.seq_num_ack = -1;  // XXX
            frame.ack = -1; // XXX
            tloe_ether_send(ether, (char *)&frame, sizeof(TloeFrame));
            element->state = TLOE_RESENT;
        }
    }

    return n;
}

// 송신 함수
void TX(TloeEther *ether, TloeFrame *frame) {
    static int tx_seq_num = 0;

    // Generate a TLoE frame
    pthread_mutex_lock(&retransmit_buffer.lock);
    if (is_queue_full(&retransmit_buffer)) {
        while (acked == 0) 
            pthread_cond_wait(&retransmit_buffer.cond, &retransmit_buffer.lock);
        acked = 0;
    } else {
        // The transmission of a new TLoE frame can be started if the retransmit buffer has sufficient space
        // to store the new frame and (NEXT_TX_SEQ - ACKD_SEQ) mod 2^22 < 2^21
        RetransmitBufferElement *element;
        element = (RetransmitBufferElement *) malloc(sizeof(RetransmitBufferElement));

        element->seq_num = next_tx_seq;
        element->state = TLOE_INIT;
        element->send_time = time(NULL);

        frame->seq_num = next_tx_seq;
        frame->seq_num_ack = -1;  // XXX
        frame->ack = -1; // XXX

        if (enqueue(&retransmit_buffer, element)) {
            printf("Sending a frame with seq_num: %d\n", element->seq_num);
            // During the transmission of a TLoE frame, the NEXT_TX_SEQ counter is updated as follows:
            // NEXT_TX_SEQ = (NEXT_TX_SEQ + 1) mode 2^22
            next_tx_seq = (next_tx_seq + 1) % (MAX_SEQ_NUM + 1);
            tloe_ether_send(ether, (char *) &frame, sizeof(TloeFrame));
            element->state = TLOE_SENT;
        } else {
            printf("ETTOR: %s: %d\n", __FILE__, __LINE__); // ERROR case
            exit(1);
        }
    }
    pthread_mutex_unlock(&retransmit_buffer.lock);
}

void *RX(void *arg) {
    TloeEther *ether = (TloeEther *)arg;

    while (1) {
        int size;
        TloeFrame frame;
        RetransmitBufferElement *element;

        pthread_mutex_lock(&rx_buffer.lock);

        size = tloe_ether_recv(ether, (char *) &frame);
        printf("RX: seq_num: %d, ack: %d\n", frame.seq_num, frame.ack);

        pthread_mutex_lock(&retransmit_buffer.lock);
        if (frame.ack == 0) { // NACK(negative ack)
            retransmit(ether, frame.seq_num_ack);
        } else if (frame.ack == 1) { // ACK(positive ack)
            element = (RetransmitBufferElement *) dequeue(&retransmit_buffer);
            acked_seq = frame.seq_num_ack;
            acked = 1;
            pthread_cond_signal(&retransmit_buffer.cond);
            printf("RX: frame.seq_num_ack: %d, element->seq_num: %d\n", frame.seq_num_ack, element->seq_num);
        }

        if (element)
            free(element);
        pthread_mutex_unlock(&retransmit_buffer.lock);

        pthread_mutex_unlock(&rx_buffer.lock);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    pthread_t rx_thread;
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

    initQueue(&retransmit_buffer);
    initQueue(&rx_buffer);

    // Create threads
    pthread_create(&rx_thread, NULL, RX, (void *) ether);

    TX(ether);

    // Join threads
    pthread_join(rx_thread, NULL);

    // Cleanup
    tloe_ether_close(ether);

    return 0;
}
