#ifndef __TLOE_RECEIVER_H__
#define __TLOE_RECEIVER_H__
#include "tloe_endpoint.h"
#include "tloe_seq_mgr.h"

typedef struct {
	int channel;
	int credit;
} chan_credit_t;

typedef enum {
    REQ_NORMAL = 0,
    REQ_DUPLICATE,
    REQ_OOS,
} tloe_rx_req_type_t;


void RX(tloe_endpoint_t *);

static inline tloe_rx_req_type_t tloe_rx_get_req_type(tloe_endpoint_t *e, tloe_frame_t *f) {
    int diff_seq = tloe_seqnum_cmp(f->header.seq_num, e->next_rx_seq);
    if (diff_seq == 0)
        return REQ_NORMAL;
    else if (diff_seq < 0)
        return REQ_DUPLICATE;
    return REQ_OOS;
}

#endif // __TLOE_RECEIVER_H__
