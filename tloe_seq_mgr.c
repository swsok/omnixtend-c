#include "tloe_seq_mgr.h"

int tloe_seqnum_cmp(uint32_t a, uint32_t b) {
    int diff = (int)a - (int)b; // 두 값의 차이 계산

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

uint32_t tloe_seqnum_prev(uint32_t seq_num) {
    int t = (int)seq_num - 1;
    if (t < 0)
        t = MAX_SEQ_NUM;
    return (uint32_t)t;
}

uint32_t tloe_seqnum_next(uint32_t seq_num) {
    return (seq_num + 1) & MAX_SEQ_NUM;
}

uint32_t tloe_seqnum_next_tx_seq_inc(tloe_endpoint_t *ep) {
    ep->next_tx_seq = tloe_seqnum_next(ep->next_tx_seq);
    return ep->next_tx_seq;
}

void tloe_seqnum_set_next_tx_seq(tloe_frame_t *frame, tloe_endpoint_t *ep) {
    frame->header.seq_num = ep->next_tx_seq;
}

void tloe_seqnum_set_seq_num_ack(tloe_frame_t *frame, tloe_endpoint_t *ep) {
    frame->header.seq_num_ack = tloe_seqnum_prev(ep->next_rx_seq);
} 

void tloe_seqnum_set_next_and_acked_seq(tloe_frame_t *frame, tloe_endpoint_t *ep) {
    // Update next_tx_seq
    tloe_seqnum_set_next_tx_seq(frame, ep);
    // Update seq_num_ack
    tloe_seqnum_set_seq_num_ack(frame, ep);
}

void tloe_seqnum_update_next_rx_seq(tloe_endpoint_t *ep, tloe_frame_t *frame) {
    ep->next_rx_seq = tloe_seqnum_next(frame->header.seq_num);
}

void tloe_seqnum_update_acked_seq(tloe_endpoint_t *ep, tloe_frame_t *frame) {
    if (tloe_seqnum_cmp(frame->header.seq_num_ack, ep->acked_seq) > 0) {
         ep->acked_seq = frame->header.seq_num_ack;
    }
}

void tloe_seqnum_set_seq_num(tloe_frame_t *frame, uint32_t seq_num) {
    frame->header.seq_num = seq_num;
}

void tloe_seqnum_set_frame_seq_num_ack(tloe_frame_t *tloeframe, uint32_t seq_num_ack) {
    tloeframe->header.seq_num_ack = seq_num_ack;
}

