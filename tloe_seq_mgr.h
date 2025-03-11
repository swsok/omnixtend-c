#ifndef __TLOE_SEQ_MGR_H__
#define __TLOE_SEQ_MGR_H__

#include "tloe_endpoint.h"
#include "tloe_frame.h"

int tloe_seqnum_cmp(uint32_t, uint32_t);
uint32_t tloe_seqnum_prev(uint32_t);
uint32_t tloe_seqnum_next(uint32_t);
uint32_t tloe_seqnum_next_tx_seq_inc(tloe_endpoint_t *);
void tloe_seqnum_set_next_tx_seq(tloe_frame_t *, tloe_endpoint_t *);
void tloe_seqnum_set_seq_num_ack(tloe_frame_t *, tloe_endpoint_t *);
void tloe_seqnum_set_next_and_acked_seq(tloe_frame_t *, tloe_endpoint_t *); 
void tloe_seqnum_update_next_rx_seq(tloe_endpoint_t *, tloe_frame_t *);
void tloe_seqnum_update_acked_seq(tloe_endpoint_t *, tloe_frame_t *);
void tloe_seqnum_set_frame_seq_num_ack(tloe_frame_t *, uint32_t);
void tloe_seqnum_set_seq_num(tloe_frame_t *, uint32_t);

#endif //__TLOE_SEQ_MGR_H__
