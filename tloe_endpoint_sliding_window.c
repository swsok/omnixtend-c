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

#define WINDOW_SIZE		4
#define MAX_SEQ_NUM		((1<<10)-1)
#define TIMEOUT_SEC		2 // 타임아웃 시간 (초)
		      
#define TLOE_INIT	(1<<0)
#define TLOE_SENT	(1<<1)
#define TLOE_RESENT		(1<<2)
#define TLOE_ACKED	(1<<3)
#define TLOE_TIMEOUT	(1<<4)

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

CircularQueue *retransmit_buffer;
CircularQueue *rx_buffer;
CircularQueue *message_buffer;

int next_tx_seq = 0;
int next_rx_seq = 0;
int acked_seq = MAX_SEQ_NUM;
int acked = 0;

static TloeEther *ether;
static int is_done = 1;

void error_exit(const char *message) {
    fprintf(stderr, "Error: %s\n", message);
    perror("System error"); // errno 기반 시스템 오류 메시지 출력
    exit(EXIT_FAILURE);
}

int retransmit(TloeEther *ether, int seq_num_nack) {
    int i, n;
    TloeFrame frame;
    // update ACKD_SEQ ????
    // retransmit 
    n = 0;
    for (i=retransmit_buffer->front; i != retransmit_buffer->rear; i++){
        RetransmitBufferElement *element = (RetransmitBufferElement *) retransmit_buffer->data[i];
        if (element->seq_num > seq_num_nack) {
            frame.seq_num = element->seq_num;
            frame.seq_num_ack = -1;  // XXX
            frame.ack = -1; // XXX
            tloe_ether_send(ether, (char *)&frame, sizeof(TloeFrame));
            element->state = TLOE_RESENT;
        }
    }

    return n;
}

int slide_window(TloeEther *ether, int seq_num_ack) {
    RetransmitBufferElement *e;

    // dequeue TLoE frames from the retransmit buffer
    e = (RetransmitBufferElement *) getfront(retransmit_buffer);
    while (e != NULL && e->seq_num <= seq_num_ack) {
        e = (RetransmitBufferElement *) dequeue(retransmit_buffer);
        printf("RX: frame.seq_num_ack: %d, element->seq_num: %d\n", seq_num_ack, e->seq_num);
        acked_seq = e->seq_num;
        if (e) free(e);
        e = (RetransmitBufferElement *) getfront(retransmit_buffer);
    }
}

void TX() {

    if (is_queue_empty(message_buffer))
        return;

    TloeFrame *tloeframe = (TloeFrame *)dequeue(message_buffer);
    if (!tloeframe) {
        printf("ERROR: %s: %d\n", __FILE__, __LINE__);
        return;
    }

    if (!is_queue_full(retransmit_buffer)) {
        // If the TileLink message is empty
        if (is_ack_msg(tloeframe)) {
            // ACK/NAK (zero-TileLink)
            // Reflect the sequence number but do not store in the retransmission buffer, just send
            tloeframe->seq_num_ack = tloeframe->seq_num;
            tloeframe->seq_num = next_tx_seq;

            printf("TX: Sending ACK/NAK with seq_num: %d, seq_num_ack: %d, ack: %d\n", 
				tloeframe->seq_num, tloeframe->seq_num_ack, tloeframe->ack);

            tloe_ether_send(ether, (char *)tloeframe, sizeof(TloeFrame));

            next_tx_seq = (next_tx_seq + 1) % (MAX_SEQ_NUM + 1);
        } else {
            // NORMAL packet
            // Reflect the sequence number, store in the retransmission buffer, and send
            // Enqueue to retransmitBuffer
            RetransmitBufferElement *e = (RetransmitBufferElement *)malloc(sizeof(RetransmitBufferElement));

            e->seq_num = next_tx_seq;
            e->state = TLOE_INIT;
            e->send_time = time(NULL);

            if (!enqueue(retransmit_buffer, e)) {
                free(e);
                printf("ERROR: %s: %d\n", __FILE__, __LINE__);
                exit(1);
            }

            tloeframe->seq_num = next_tx_seq;
            tloeframe->seq_num_ack = acked_seq;
            tloeframe->ack = 1;
            tloeframe->mask = 1;

            printf("TX: Sending TL msg with seq_num: %d, seq_num_ack: %d\n", tloeframe->seq_num, tloeframe->seq_num_ack);
            tloe_ether_send(ether, (char *)tloeframe, sizeof(TloeFrame));

            // Update sequence number
            next_tx_seq = (next_tx_seq + 1) % (MAX_SEQ_NUM + 1);
            //acked_seq = tloeframe->seq_num_ack;
            e->state = TLOE_SENT;
        }
    } else {
        printf("retransmit_buffer is full: %s: %d\n", __FILE__, __LINE__);
    }
}

void RX() {
    int size;
    TloeFrame tloeframe;

    // Receive a frame from the Ethernet layer
    size = tloe_ether_recv(ether, (char *)&tloeframe);
    if (size == -1 && errno == EAGAIN) {
        // No data available, return without processing
        //printf("File: %s line: %d: tloe_ether_recv error\n", __FILE__, __LINE__);
        return;
    }

    printf("RX: Received packet with seq_num: %d, seq_num_ack: %d, ack: %d\n", tloeframe.seq_num, tloeframe.seq_num_ack, tloeframe.ack);

    // If the received frame has the expected sequence number
    if (tloeframe.seq_num == next_rx_seq) {

        if (is_ack_msg(&tloeframe)) {
            // ACK/NAK (zero-TileLink)
            // Handle ACK/NAK and finish processing
            if (tloeframe.ack == 0) {  // NACK
                retransmit(ether, tloeframe.seq_num_ack);
            } else if (tloeframe.ack == 1) {  // ACK
                slide_window(ether, tloeframe.seq_num_ack);
            }

            // Update sequence numbers
            next_rx_seq = (tloeframe.seq_num + 1) % (MAX_SEQ_NUM + 1);
            acked_seq = tloeframe.seq_num_ack;

            printf("Done! next_tx: %d, ackd: %d, next_rx: %d\n", next_tx_seq, acked_seq, next_rx_seq);
        } else {
            // Normal request packet
            // Handle and enqueue it into the message buffer
            TloeFrame *frame = malloc(sizeof(TloeFrame));

            frame->seq_num = tloeframe.seq_num;
            frame->seq_num_ack = tloeframe.seq_num_ack;
            frame->ack = tloeframe.ack;
            frame->mask = 0;				// To indicate ACK

            // Handle TileLink Msg
			printf("RX: Send pakcet to Tx channel for replying ACK/NAK with seq_num: %d, seq_num_ack: %d, ack: %d\n", 
				tloeframe.seq_num, tloeframe.seq_num_ack, tloeframe.ack);

            if (!enqueue(message_buffer, (void *) frame)) {
                printf("File: %s line: %d: enqueue error\n", __FILE__, __LINE__);
                exit(1);
            }

            // Update sequence numbers
            next_rx_seq = (tloeframe.seq_num + 1) % (MAX_SEQ_NUM+1);
            acked_seq = tloeframe.seq_num_ack;
        }

    } else if (((MAX_SEQ_NUM + 1) + next_rx_seq - tloeframe.seq_num) % (MAX_SEQ_NUM + 1) <= (MAX_SEQ_NUM) / 2) {
        // The received TLoE frame is a duplicate
        // The frame should be dropped, NEXT_RX_SEQ is not updated
        // A positive acknowledgment is sent using the received sequence number
        int seq_num = tloeframe.seq_num;
        printf("TLoE frame is a duplicate. seq_num: %d, next_rx_seq: %d\n", seq_num, next_rx_seq);
        tloeframe.ack = 1;
        tloeframe.seq_num_ack = seq_num;

        // If the received frame contains data, enqueue it in the message buffer
        if (tloe_get_mask(&tloeframe)) {
            if (!enqueue(message_buffer, (void *) &tloeframe)) {
                printf("File: %s line: %d: enqueue error\n", __FILE__, __LINE__);
                exit(1);
            }
        }
    } else {
        // The received TLoE frame is out of sequence, indicating that some frames were lost
        // The frame should be dropped, NEXT_RX_SEQ is not updated
        // A negative acknowledgment (NACK) is sent using the last properly received sequence number
        printf("TLoE frame is out of sequence\n");
        tloeframe.seq_num_ack = next_rx_seq;
        tloeframe.ack = 0;  // NACK

        // If the received frame contains data, enqueue it in the message buffer
        if (tloe_get_mask(&tloeframe)) {
            if (!enqueue(message_buffer, (void *) &tloeframe)) {
                printf("File: %s line: %d: enqueue error\n", __FILE__, __LINE__);
                exit(1);
            }
        }
    }
}

void *tloe_endpoint(void *arg) {

	while(is_done) {
		TX();
		RX();
	}
	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    pthread_t tloe_endpoint_thread;
    int master_slave = 1;
	char input;

    if (argc < 3) {
        printf("Usage: tloe_endpoint queue_name master[1]/slave[0]\n");
        exit(0);
    }


    if (argv[2][0] == '1')
        ether = tloe_ether_open(argv[1], 1);
    else
        ether = tloe_ether_open(argv[1], 0);

    retransmit_buffer = create_queue(WINDOW_SIZE + 1);
    rx_buffer = create_queue(10); // credits
	message_buffer = create_queue(10);

	if (pthread_create(&tloe_endpoint_thread, NULL, tloe_endpoint, NULL) != 0) {
        error_exit("Failed to ack reply thread");
    }
    
	printf("Enter 's' to send a message, 'q' to quit:\n");
	while(1) {
		printf("> ");
		scanf(" %c", &input);

		if (input == 's') {
			TloeFrame *new_tloe = (TloeFrame *)malloc(sizeof(TloeFrame));
			if (!new_tloe) {
				printf("Memory allocation failed!\n");
				continue;
			}
			new_tloe->mask = 1;

			if (enqueue(message_buffer, new_tloe)) {
				printf("Message added to message_buffer\n");
			} else {
				printf("Failed to enqueue message, buffer is full.\n");
				free(new_tloe);
			}
		} else if (input == 'a') {
			for (int i = 0; i < 10; i++) {
				TloeFrame *new_tloe = (TloeFrame *)malloc(sizeof(TloeFrame));
				if (!new_tloe) {
					printf("Memory allocation failed at packet %d!\n", i);
					continue;
				}

				new_tloe->mask = 1;  // Set mask (1 = normal packet)

				if (enqueue(message_buffer, new_tloe)) {
					printf("Packet %d added to message_buffer\n", i);
				} else {
					printf("Failed to enqueue packet %d, buffer is full.\n", i);
					free(new_tloe);
					break;  // Stop if buffer is full
				}
			}
		} else if (input == 'q') {
			printf("Exiting...\n");
			break;
		}
	}

    // Join threads
    pthread_join(tloe_endpoint_thread, NULL);

    // Cleanup
    tloe_ether_close(ether);

    return 0;
}

