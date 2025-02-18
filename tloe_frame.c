#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tloe_frame.h"

int tloe_set_seq_num(tloe_frame_t *frame, int seq_num) {
    frame->header.seq_num = seq_num;
    return seq_num;
}

int tloe_get_seq_num(tloe_frame_t *frame) {
    return frame->header.seq_num;
}

int tloe_set_seq_num_ack(tloe_frame_t *frame, int seq_num) {
    frame->header.seq_num_ack = seq_num;
    return seq_num;
}

int tloe_get_seq_num_ack(tloe_frame_t *frame) {
    return frame->header.seq_num_ack;
}

int tloe_set_ack(tloe_frame_t *frame, int ack) {
    frame->header.ack = ack;
    return ack;
}

int tloe_get_ack(tloe_frame_t *frame) {
    return frame->header.ack;
}

int tloe_set_mask(tloe_frame_t *frame, int mask) {
	frame->flits[MAX_SIZE_FLIT-1] = mask;
    return mask;
}

int tloe_get_mask(tloe_frame_t *frame) {
	return frame->flits[MAX_SIZE_FLIT-1];
}

void tloe_get_tlmsg(tloe_frame_t *frame, tl_msg_t *tlmsg, int loc) {
	memcpy(tlmsg, &frame->flits[loc], sizeof(uint64_t));
}

void tloe_set_tlmsg(tloe_frame_t *frame, tl_msg_t *tlmsg, int loc) {
	memcpy(&frame->flits[loc], tlmsg, sizeof(uint64_t));
}

int is_ack_msg(tloe_frame_t *frame) {
	return (frame->flits[MAX_SIZE_FLIT-1] == 0);
}

int is_conn_msg(tloe_frame_t *frame) {
	return (frame->header.type == 2);
}
