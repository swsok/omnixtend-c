#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include "tloe_ether.h"
#include "tloe_frame.h"
#include "tilelink_msg.h"
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
void TX(TloeEther *ether, TileLinkMsg *tlmsg) {
    TloeFrame tloeframe;
    size_t size;
    RetransmitBufferElement *e;

    pthread_mutex_lock(&retransmit_buffer.lock);
    if (!is_queue_full(&retransmit_buffer)) {
        // The transmission of a new TLoE frame can be started if the retransmit buffer has sufficient space
        // to store the new frame and (NEXT_TX_SEQ - ACKD_SEQ) mod 2^22 < 2^21
        e = (RetransmitBufferElement *) malloc(sizeof(RetransmitBufferElement));

        e->seq_num = next_tx_seq;
        e->state = TLOE_INIT;
        e->send_time = time(NULL);

        if (!enqueue(&retransmit_buffer, e)) {
            pthread_mutex_unlock(&retransmit_buffer.lock);
            printf("ERROR: %s: %d\n", __FILE__, __LINE__); // ERROR case
            exit(1); // just for debugging
            return;
        } 
        printf("Sending a frame with seq_num: %d\n", e->seq_num);
        tloeframe.seq_num = next_tx_seq;
        tloeframe.seq_num_ack = -1;  // XXX
        tloeframe.ack = -1; // XXX
        //XXX: Add TileLink message(tlmsg) to the TLoE frame

        tloe_ether_send(ether, (char *)&tloeframe, sizeof(TloeFrame));
        // During the transmission of a TLoE frame, the NEXT_TX_SEQ counter is updated as follows:
        // NEXT_TX_SEQ = (NEXT_TX_SEQ + 1) mod 2^22
        next_tx_seq = (next_tx_seq + 1) % (MAX_SEQ_NUM + 1);
        e->state = TLOE_SENT;
    }
    pthread_mutex_unlock(&retransmit_buffer.lock);

    // RX
    do {
        size = tloe_ether_recv(ether, (char *) &tloeframe);
        if (size == -1 && errno == EAGAIN) // non-blocking mode, no data to receive
            break;

        printf("RX: seq_num: %d, ack: %d\n", tloeframe.seq_num_ack, tloeframe.ack);

        pthread_mutex_lock(&retransmit_buffer.lock);
        if (tloeframe.ack == 0) { // NACK(negative ack)
            retransmit(ether, tloeframe.seq_num_ack);
        } else if (tloeframe.ack == 1) { // ACK(positive ack)
            // dequeue TLoE frames from the retransmission buffer
            e = (RetransmitBufferElement *) dequeue(&retransmit_buffer);
            printf("RX: frame.seq_num_ack: %d, element->seq_num: %d\n", tloeframe.seq_num_ack, e->seq_num);
            acked_seq = e->seq_num;
            if (e) free(e);
        }
        pthread_mutex_unlock(&retransmit_buffer.lock);
    } while (size > 0);

}

void *receiver(void *arg) {
    TloeEther *ether = (TloeEther *)arg;

    while (1) {
        int size;
        TloeFrame frame;
        RetransmitBufferElement *element;

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

    init_queue(&retransmit_buffer);
    init_queue(&rx_buffer);

    // Create threads
    pthread_create(&rx_thread, NULL, receiver, (void *) ether);

    while (1) {
        TileLinkMsg tlmsg;

    //   Generate a TileLink message, GET/PUT/...
        TX(ether, &tlmsg);
    }

    // Join threads
    pthread_join(rx_thread, NULL);

    // Cleanup
    tloe_ether_close(ether);

    return 0;
}
