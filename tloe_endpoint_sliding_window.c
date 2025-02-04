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
CircularQueue *ack_buffer;

int next_tx_seq = 0;
int next_rx_seq = 0;
int acked_seq = MAX_SEQ_NUM;
int acked = 0;

int ack_cnt = 0;

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
    for (i=retransmit_buffer->front; i != retransmit_buffer->rear; i = (i + 1) % retransmit_buffer->size){
        RetransmitBufferElement *element = (RetransmitBufferElement *) retransmit_buffer->data[i];
        if (element->tloe_frame.seq_num > seq_num_nack) {

			frame = element->tloe_frame;
			frame.mask = 1;		// Indicate to normal packet

			printf("Retransmission with num_seq: %d\n", frame.seq_num);
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
    while (e != NULL && e->tloe_frame.seq_num <= seq_num_ack) {
        e = (RetransmitBufferElement *) dequeue(retransmit_buffer);
//        printf("RX: frame.seq_num_ack: %d, element->seq_num: %d\n", seq_num_ack, e->tloe_frame.seq_num);
        acked_seq = e->tloe_frame.seq_num;
        if (e) free(e);
        e = (RetransmitBufferElement *) getfront(retransmit_buffer);
    }
}

TloeFrame *TX(TloeFrame *tloeframe) {
	TloeFrame *returnframe = NULL;
    if (!is_queue_empty(ack_buffer)) {
		TloeFrame *ackframe = NULL;

		returnframe = tloeframe;
		ackframe = (TloeFrame *)dequeue(ack_buffer);

		// ACK/NAK (zero-TileLink)
		// Reflect the sequence number but do not store in the retransmission buffer, just send
		ackframe->seq_num_ack = ackframe->seq_num;
		ackframe->seq_num = next_tx_seq;

		//            printf("TX: Sending ACK/NAK with seq_num: %d, seq_num_ack: %d, ack: %d\n", 
		//				tloeframe->seq_num, tloeframe->seq_num_ack, tloeframe->ack);

		tloe_ether_send(ether, (char *)ackframe, sizeof(TloeFrame));

		next_tx_seq = (next_tx_seq + 1) % (MAX_SEQ_NUM + 1);

		free(ackframe);
	} else if (tloeframe == NULL) {
	} else if (!is_queue_full(retransmit_buffer)) {
        // If the TileLink message is empty
        if (is_ack_msg(tloeframe)) {
			
			printf("ERROR: %s: %d\n", __FILE__, __LINE__);
			exit(1);
       } else {
            // NORMAL packet
            // Reflect the sequence number, store in the retransmission buffer, and send
            // Enqueue to retransmitBuffer
            RetransmitBufferElement *e = (RetransmitBufferElement *)malloc(sizeof(RetransmitBufferElement));

            e->tloe_frame.seq_num = next_tx_seq;
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

            // Update sequence number
            next_tx_seq = (next_tx_seq + 1) % (MAX_SEQ_NUM + 1);

#if 0
			// DEBUG: DROP packet
			if (next_tx_seq == 5) {
				printf("TX: DROPPED packet for seq_num: %d\n", next_tx_seq);
				return ;
			}
#endif

 //           printf("TX: Sending TL msg with seq_num: %d, seq_num_ack: %d\n", tloeframe->seq_num, tloeframe->seq_num_ack);
            tloe_ether_send(ether, (char *)tloeframe, sizeof(TloeFrame));

            e->state = TLOE_SENT;
        }
    } else {
        //printf("retransmit_buffer is full: %s: %d\n", __FILE__, __LINE__);
		returnframe = tloeframe;
    }

#if 0
	e = getfront(retransmit_buffer);
	if (!e) return;

	if ((time(NULL)) - e->send_time >= TIMEOUT_SEC)
		tx_retransmit(ether, e->seq_num);
#endif

	return returnframe;
}

#if 0
time_t last_ack_time = 0;
#endif

void RX() {
    int size;
    TloeFrame *tloeframe = (TloeFrame *)malloc(sizeof(TloeFrame));

//	if ((time(NULL) - last_ack_time) >= ACK_TIMEOUT_SEC;

    // Receive a frame from the Ethernet layer
    size = tloe_ether_recv(ether, (char *)tloeframe);
    if (size == -1 && errno == EAGAIN) {
        // No data available, return without processing
        //printf("File: %s line: %d: tloe_ether_recv error\n", __FILE__, __LINE__);
		free(tloeframe);
        return;
    }

  //  printf("RX: Received packet with seq_num: %d, seq_num_ack: %d, ack: %d\n", tloeframe->seq_num, tloeframe->seq_num_ack, tloeframe->ack);

	int diff_seq = next_rx_seq - tloeframe->seq_num;
	if (diff_seq < 0)
		diff_seq += MAX_SEQ_NUM + 1;

    // If the received frame has the expected sequence number
    if (diff_seq == 0) {
        if (is_ack_msg(tloeframe)) {
            // ACK/NAK (zero-TileLink)
             slide_window(ether, tloeframe->seq_num_ack);

            // Handle ACK/NAK and finish processing
            if (tloeframe->ack == 0) {  // NACK
                retransmit(ether, tloeframe->seq_num_ack);
            }

            // Update sequence numbers
            next_rx_seq = (tloeframe->seq_num + 1) % (MAX_SEQ_NUM + 1);
            acked_seq = tloeframe->seq_num_ack;
			ack_cnt++;

			free(tloeframe);

			if (ack_cnt % 100 == 0) {
				fprintf(stderr, "next_tx: %d, ackd: %d, next_rx: %d, ack_cnt: %d\n", next_tx_seq, acked_seq, next_rx_seq, ack_cnt);
			}
        } else {
            // Normal request packet
            // Handle and enqueue it into the message buffer
            TloeFrame *frame = malloc(sizeof(TloeFrame));

			*frame = *tloeframe;
            frame->mask = 0;				// To indicate ACK

            // Handle TileLink Msg
//			printf("RX: Send pakcet to Tx channel for replying ACK/NAK with seq_num: %d, seq_num_ack: %d, ack: %d\n", 
//				tloeframe->seq_num, tloeframe->seq_num_ack, tloeframe->ack);

            if (!enqueue(ack_buffer, (void *) frame)) {
                printf("File: %s line: %d: enqueue error\n", __FILE__, __LINE__);
                exit(1);
            }

            // Update sequence numbers
            next_rx_seq = (tloeframe->seq_num + 1) % (MAX_SEQ_NUM+1);
            acked_seq = tloeframe->seq_num_ack;
        }
		//if (next_tx_seq % 100 == 0)
		//	fprintf(stderr, "next_tx: %d, ackd: %d, next_rx: %d, ack_cnt: %d\n", next_tx_seq, acked_seq, next_rx_seq, ack_cnt);
	} else if (diff_seq < (MAX_SEQ_NUM + 1) / 2) {
        // The received TLoE frame is a duplicate
        // The frame should be dropped, NEXT_RX_SEQ is not updated
        // A positive acknowledgment is sent using the received sequence number
        int seq_num = tloeframe->seq_num;
        printf("TLoE frame is a duplicate. seq_num: %d, next_rx_seq: %d\n", seq_num, next_rx_seq);

		// If the received frame contains data, enqueue it in the message buffer

		if (is_ack_msg(tloeframe)) {
			printf("File: %s line: %d: duplication error\n", __FILE__, __LINE__);
			exit(1);
			free(tloeframe);
		}

		tloeframe->seq_num_ack = seq_num;
		tloeframe->ack = 1;
		tloeframe->mask = 0; // To indicate ACK

		if (!enqueue(ack_buffer, (void *) tloeframe)) {
			printf("File: %s line: %d: enqueue error\n", __FILE__, __LINE__);
			exit(1);
		}
	} else {

        // The received TLoE frame is out of sequence, indicating that some frames were lost
        // The frame should be dropped, NEXT_RX_SEQ is not updated
        // A negative acknowledgment (NACK) is sent using the last properly received sequence number
		int last_proper_rx_seq = (next_rx_seq - 1) % (MAX_SEQ_NUM+1);
		if (last_proper_rx_seq < 0)
			last_proper_rx_seq += MAX_SEQ_NUM + 1;

        printf("TLoE frame is out of sequence with seq_num: %d, next_rx_seq: %d\n", tloeframe->seq_num, next_rx_seq);

        // If the received frame contains data, enqueue it in the message buffer
		if (!is_ack_msg(tloeframe)) {
			tloeframe->seq_num = last_proper_rx_seq;
			tloeframe->ack = 0;  // NACK
			tloeframe->mask = 0; // To indicate ACK

			if (!enqueue(ack_buffer, (void *) tloeframe)) {
                printf("File: %s line: %d: enqueue error\n", __FILE__, __LINE__);
                exit(1);
            }
        } else {
			free(tloeframe);
		}
    }
}

void *tloe_endpoint(void *arg) {
	TloeFrame *tloeframe = NULL;
	TloeFrame *t = NULL;

	while(is_done) {
		if (tloeframe == NULL && !is_queue_empty(message_buffer)) 
			tloeframe = t = dequeue(message_buffer);
		tloeframe = TX(tloeframe);
		if (tloeframe == NULL && t != NULL) { 
			free(t);
			t = NULL;
		}
		RX();
	}
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
	message_buffer = create_queue(10000);
	ack_buffer = create_queue(100);

	if (pthread_create(&tloe_endpoint_thread, NULL, tloe_endpoint, NULL) != 0) {
        error_exit("Failed to ack reply thread");
    }
    
	while(1) {
		printf("Enter 's' to send a message, 'q' to quit:\n");
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
			for (int i = 0; i < 1000000000; i++) {
				TloeFrame *new_tloe = (TloeFrame *)malloc(sizeof(TloeFrame));
				if (!new_tloe) {
					printf("Memory allocation failed at packet %d!\n", i);
					continue;
				}

				new_tloe->mask = 1;  // Set mask (1 = normal packet)

				while(is_queue_full(message_buffer)) 
					usleep(1000);

				if (enqueue(message_buffer, new_tloe)) {
					if (i % 100 == 0)
						printf("Packet %d added to message_buffer\n", i);
				} else {
					//printf("Failed to enqueue packet %d, buffer is full.\n", i);
					free(new_tloe);
					break;  // Stop if buffer is full
				}
			}
		} else if (input == 'q') {
			printf("Exiting...\n");
			break;
		}
	}

	is_done = 1;

    // Join threads
    pthread_join(tloe_endpoint_thread, NULL);

    // Cleanup
    tloe_ether_close(ether);

    return 0;
}

