#ifndef __TLOE_ENDPOINT_H__
#define __TLOE_ENDPOINT_H__

#include <stdint.h>
#include "tloe_fabric.h"
#include "tloe_ether.h"
#include "tloe_frame.h"
#include "timeout.h"
#include "flowcontrol.h"
#include "util/circular_queue.h"

#define MAX_SEQ_NUM      ((uint32_t)(1<<22)-1)
#define HALF_MAX_SEQ_NUM ((MAX_SEQ_NUM + 1)/2)
#define MAX_BUFFER_SIZE  1500
#define TYPE_MASTER      1
#define TYPE_SLAVE       0

typedef struct tloe_endpoint_struct {
	tloe_fabric_ops_t fabric_ops; // Current media operations in use

	CircularQueue *retransmit_buffer;
	CircularQueue *message_buffer;
	CircularQueue *ack_buffer;
	CircularQueue *tl_msg_buffer;
	CircularQueue *response_buffer;

	struct timespec iteration_ts;

	flowcontrol_t fc;

    pthread_t tloe_endpoint_thread;

	volatile int connection;
	int master;
	volatile int is_done;
    int fabric_type;

	volatile int next_tx_seq;
	volatile int next_rx_seq;
	volatile uint32_t acked_seq;

    bool should_send_ackonly_frame;
    bool ackonly_frame_sent;

	int ack_cnt;
	int dup_cnt;
	int oos_cnt;
	int drop_cnt;

    int accessack_cnt;
    int accessackdata_cnt;

    int ackonly_cnt;

	int drop_npacket_size;
	int drop_npacket_cnt;

	int drop_tlmsg_cnt;
	int drop_response_cnt;
} tloe_endpoint_t;

static inline void print_payload(char *data, int size) {
    int i, j;

    printf("\n");
    for (i = 0; i < size; i++) {
        if (i != 0 && i % 16 == 0) {
            printf("\t");
            for (j = i - 16; j < i; j++) {
                if (data[j] >= 32 && data[j] < 128)
                    printf("%c", (unsigned char)data[j]);
                else
                    printf(".");
            }
            printf("\n");
        }

        if ( (i % 8) == 0 && (i % 16) != 0 ) printf(" ");
        printf(" %02X", (unsigned char) data[i]);       // print DATA

        if (i == size - 1) {
            for (j = 0; j < (15 - (i % 16)); j++)
                printf("   ");

            printf("\t");

            for (j = (i - (i % 16)); j <= i; j++) {
                if (data[j] >= 32 && data[j] < 128)
                    printf("%c", (unsigned char) data[j]);
                else
                    printf(".");
            }
            printf("\n");
        }
    }
    printf("\n");
}

#endif // __TLOE_ENDPOINT_H__
