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
	CircularQueue *rx_buffer;
	CircularQueue *message_buffer;
	CircularQueue *ack_buffer;
	CircularQueue *tl_msg_buffer;
	CircularQueue *response_buffer;

	timeout_t timeout_rx;
	struct timespec iteration_ts;

	flowcontrol_t fc;

    pthread_t tloe_endpoint_thread;

	int connection;
	int master;
	int is_done;
    int fabric_type;

	int next_tx_seq;
	int next_rx_seq;
	uint32_t acked_seq;
	int acked;

    int should_send_ackonly_frame;
    bool ackonly_frame_sent;

	int ack_cnt;
	int dup_cnt;
	int oos_cnt;
	int delay_cnt;
	int drop_cnt;

	int drop_npacket_size;
	int drop_npacket_cnt;
	int drop_apacket_size;
	int drop_apacket_cnt;

	int fc_inc_cnt;
	int fc_inc_value;
	int fc_dec_cnt;
	int fc_dec_value;

	int drop_tlmsg_cnt;
	int drop_response_cnt;

    int accessack_cnt;
    int accessackdata_cnt;

    int close_flag;

    int ackonly_cnt;
} tloe_endpoint_t;

typedef enum {
	REQ_NORMAL = 0,
	REQ_DUPLICATE,
	REQ_OOS,
} tloe_rx_req_type_t;

static inline int tloe_seqnum_cmp(uint32_t a, uint32_t b) {
    int diff = (int)a - (int)b;  // 두 값의 차이 계산

    if (diff == 0) {
        return 0;  // 두 값이 동일
    }
    
    // wrap-around 판단: HALF_MAX_SEQ_NUM 기준으로 앞/뒤 판별
    if (diff > 0) {
        return (diff < HALF_MAX_SEQ_NUM) ? 1 : -1;    // b(100)...a(200);   a(1020)...b(3)
    } else {
        return (-diff < HALF_MAX_SEQ_NUM) ? -1 : 1;
    }
}

static inline uint32_t tloe_seqnum_prev(uint32_t seq_num) {
	int t = (int)seq_num - 1;
	if (t < 0) 
		t = MAX_SEQ_NUM;
	return (uint32_t)t;
}

static inline uint32_t tloe_seqnum_next(uint32_t seq_num) {
	return (seq_num + 1) & MAX_SEQ_NUM;
}

static inline tloe_rx_req_type_t tloe_rx_get_req_type(tloe_endpoint_t *e, tloe_frame_t *f) {
	int diff_seq = tloe_seqnum_cmp(f->header.seq_num, e->next_rx_seq);
	if (diff_seq == 0)
		return REQ_NORMAL;
	else if (diff_seq < 0)
		return REQ_DUPLICATE;
	return REQ_OOS;
}

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
