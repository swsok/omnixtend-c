#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "tloe_ether.h"
#include "tloe_frame.h"

#define WINDOW_SIZE 	4
#define MAX_SEQ_NUM 	((1<<10)-1)
#define TIMEOUT_SEC 	2 // 타임아웃 시간 (초)
		      
#define TLOE_INIT 	(1<<0)
#define TLOE_SENT 	(1<<1)
#define TLOE_ACKED 	(1<<2)
#define TLOE_TIMEOUT 	(1<<3)

// 타임아웃 발생 여부 확인 함수
bool isTimeout(time_t send_time) {
    return difftime(time(NULL), send_time) >= TIMEOUT_SEC;
}

int next_tx_seq = 0;
int next_rx_seq = 0;
int acked_seq = MAX_SEQ_NUM;
int last_proper_rx_seq = -1;

int main(int argc, char *argv[]) {
    TloeEther *ether;
    int master_slave = 1;
    char in[1024];

    if (argc < 3) {
        printf("Usage: tloe_endpoint queue_name master[1]/slave[0]\n");
        exit(0);
    }

    if (argv[2][0] == '1')
        ether = tloe_ether_open(argv[1], 1);
    else
        ether = tloe_ether_open(argv[1], 0);

    srand(time(NULL));

    while (1) {
        size_t size;
        TloeFrame frame;

        size = tloe_ether_recv(ether, (char *)&frame);
        if (size == -1 && errno == EAGAIN) {
            printf("File: %s line: %d: tloe_ether_recv error\n",__FILE__, __LINE__);
            usleep(200000);
            continue;
        }
        /* for testing packet drop
        if (rand() % 4 == 0) { // 1/4 
            printf("A frame is dropped: %d\n", frame.seq_num);
            continue; // intentional drop for testing
        }
        */
        printf("RX: seq_num: %d\n", frame.seq_num);
        if (frame.seq_num == next_rx_seq) { // expected seq number
            last_proper_rx_seq = next_rx_seq;
            // TLOE frame is processed
            // NEXT_RX_SEQ is updated, and a positive ack is sent using the received sequence number.
            next_rx_seq = (next_rx_seq + 1) % (MAX_SEQ_NUM+1);
            frame.seq_num_ack = frame.seq_num; //
            frame.ack = 1;  // ACK
            size = tloe_ether_send(ether, (char*) &frame, sizeof(TloeFrame));
        } else if ((next_rx_seq - frame.seq_num) % (MAX_SEQ_NUM+1) <= (MAX_SEQ_NUM)/2) {
            // TLOE frame is a duplicate
            // The received TLoE frame is dropped
            // NEXT_RX_SEQ is not updated, and a positive acknowledge is sent using the received sequence number.
            int seq_num = frame.seq_num;
            printf("TLoE frame is a duplicate. seq_num: %d, next_rx_seq: %d, last_proper_rx_seq:%d\n", 
                    seq_num, next_rx_seq, last_proper_rx_seq);
            frame.ack = 1;
            frame.seq_num_ack = seq_num;
            tloe_ether_send(ether, (char*) &frame, sizeof(TloeFrame));
        } else { 
            // the received TLoE frame is out of sequence, indicating that one or more TLoE frames have been lost.
            // The received TLoE frame is dropped, the NEXT_RX_SEQ is not updated, and a negative ack is sent
            // using the last sequence number that was received in proper sequence order.
            printf("TLoE frame is out of sequence\n");
            exit(1);  // just for debugging
            frame.seq_num_ack = last_proper_rx_seq; 
            frame.ack = 0;  // NACK
            tloe_ether_send(ether, (char*) &frame, sizeof(TloeFrame));
        }
    }

    tloe_ether_close(ether);

    return 0;
}
