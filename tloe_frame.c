#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tloe_frame.h"
#include "tloe_common.h"
#include "tilelink_msg.h"

void set_tloe_frame(tloe_frame_t *tloeframe, tl_msg_t *tlmsg, uint32_t seq_num, uint32_t seq_num_ack, uint8_t ack, uint8_t chan, uint8_t credit) {
    tloeframe->header.seq_num = seq_num;
    tloeframe->header.seq_num_ack = seq_num_ack;
    tloeframe->header.ack = ack;
    tloeframe->header.chan = chan;
    tloeframe->header.credit = credit;
	tloe_set_tlmsg(tloeframe, tlmsg, 0);
}

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

uint64_t tloe_set_mask(tloe_frame_t *frame, int mask, int size) {
    frame->flits[(size >> 3) - 1 - 1] = mask;
    return mask;
}

uint64_t tloe_get_mask(tloe_frame_t *frame, int size) {
    return frame->flits[(size >> 3) - 1 - 1];
}

int tloe_get_fsize(tl_msg_t *tlmsg) {
    // TODO Need to apply other message types (Currently, only GET and PULLPULLDATA are applied).
    if (tlmsg->header.chan == CHANNEL_A && tlmsg->header.opcode == A_GET_OPCODE)
        return sizeof(uint64_t) * 7;
    else if (tlmsg->header.chan == CHANNEL_A && tlmsg->header.opcode == A_PUTFULLDATA_OPCODE)
#if 0
        return sizeof(uint64_t) * 7 + (((1ULL << tlmsg->header.size) + 7 ) / 8);
#else
        return sizeof(uint64_t) * 7;
#endif
}

tl_msg_t *tloe_get_tlmsg(tloe_frame_t *frame, int tl_loc) {
    int data_size;
    int tlmsg_chan, tlmsg_opcode, tlmsg_size;
    tl_msg_t *tlmsg;

    // Put data into tlmsg
    tl_header_t *tlheader = (tl_header_t*) &frame->flits[tl_loc];
    tlmsg_chan = tlheader->chan;
    tlmsg_opcode = tlheader->opcode;
    tlmsg_size = tlheader->size;

    // calculate size of data
    switch (tlmsg_chan) {
        case CHANNEL_A:
            if (tlmsg_opcode == A_GET_OPCODE) 
                data_size = 1;
            else if (tlmsg_opcode == A_PUTFULLDATA_OPCODE) 
                data_size = 1 + (((1ULL << tlmsg_size) + 7 ) / 8);
            break;
        case CHANNEL_D:
            if (tlmsg_opcode == D_ACCESSACK_OPCODE)
                data_size = 0;
            else if (tlmsg_opcode == D_ACCESSACKDATA_OPCODE) 
                data_size = 1 + (((1ULL << tlmsg_size) + 7 ) / 8);
            break;
    }

    size_t total_size = sizeof(tl_msg_t) + (data_size * sizeof(uint64_t));
    tlmsg = (tl_msg_t *)malloc(total_size);
    BUG_ON(!tlmsg, "malloc failed");

    memcpy(tlmsg, &frame->flits[tl_loc], total_size);

    return tlmsg;
}

int tloe_get_tlmsg_size(tl_msg_t *tlmsg) {
    int data_size;

    switch (tlmsg->header.chan) {
        case CHANNEL_A:
            if (tlmsg->header.opcode == A_GET_OPCODE) 
                data_size = 1;
            else if (tlmsg->header.opcode == A_PUTFULLDATA_OPCODE) 
                data_size = 1 + (((1ULL << tlmsg->header.size) + 7 ) / 8);
            break;
        case CHANNEL_D:
            if (tlmsg->header.opcode == D_ACCESSACK_OPCODE)
                data_size = 0;
            else if (tlmsg->header.opcode == D_ACCESSACKDATA_OPCODE) 
                data_size = 1 + (((1ULL << tlmsg->header.size) + 7 ) / 8);
            break;
    }

    return data_size;
}

void tloe_set_tlmsg(tloe_frame_t *frame, tl_msg_t *tlmsg, int tl_size) {
	memcpy(&frame->flits[0], tlmsg, sizeof(uint64_t) + sizeof(uint64_t) * tl_size);
}

int is_zero_tl_frame(tloe_frame_t *frame, int size) {
    uint64_t frame_mask = frame->flits[(size >> 3) - 1 - 1];

	return frame_mask == 0;
}

int is_ackonly_frame(tloe_frame_t *frame) {
    return (frame->header.type == TYPE_ACKONLY);
}

int is_conn_msg(tloe_frame_t *frame) {
    return (frame->header.type == 2 || frame->header.type == 3) ? frame->header.type : -1;
}

void tloe_frame_to_packet(tloe_frame_t *tloeframe, char *send_buffer, int send_buffer_size) {
    uint64_t be64_temp;
    int offset = 0;
    int flit_cnt = (send_buffer_size - 8) >> 3;

    memset((void*)send_buffer, 0, send_buffer_size);

    // TLoE frame Header
	be64_temp = htobe64(*(uint64_t*)&(tloeframe->header));
	memcpy(send_buffer+offset, &be64_temp, sizeof(uint64_t));
	offset += sizeof(uint64_t);

    // Flits 데이터 변환
    for (int i = 0; i < flit_cnt; i++) {
        be64_temp = tloeframe->flits[i];
        be64_temp = htobe64(be64_temp);  // 워드 단위 big-endian 변환
        memcpy(send_buffer+offset, &be64_temp, sizeof(uint64_t));
        offset += sizeof(uint64_t);
    }

}

void packet_to_tloe_frame(char *recv_buffer, int recv_buffer_size, tloe_frame_t *tloeframe) {
    uint64_t be64_temp;
    int offset = 0;
    int num_flits = (recv_buffer_size >> 3) - 1;

    memcpy(&be64_temp, recv_buffer+offset, sizeof(uint64_t));
    be64_temp = be64toh(be64_temp);  // Big-endian → Host-endian 변환
    memcpy(&(tloeframe->header), &be64_temp, sizeof(tloe_header_t)); // 변환된 값 구조체에 복사
    offset += sizeof(uint64_t);

    // Flits 데이터 변환
    for (int i = 0; i < num_flits; i++) {
        memcpy(&be64_temp, recv_buffer + offset, sizeof(uint64_t));
        be64_temp = be64toh(be64_temp);  // big-endian에서 호스트 순서로 변환
        tloeframe->flits[i] = be64_temp;
        offset += sizeof(uint64_t);
    }

}
