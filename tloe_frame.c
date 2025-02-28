#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tloe_frame.h"
#include "tloe_common.h"

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

int tloe_set_mask(tloe_frame_t *frame, int mask, int size) {
#if 0
    frame->flits[MAX_SIZE_FLIT-1] = mask;
    return mask;
#endif
    frame->flits[(size >> 3) - 1 - 1] = mask;
    return mask;
}

int tloe_get_mask(tloe_frame_t *frame, int size) {
#if 0
    return frame->flits[MAX_SIZE_FLIT-1];
#endif
    return frame->flits[(size >> 3) - 1 - 1];
}

int tloe_get_fsize(tl_msg_t *tlmsg) {
    // TODO
    if (tlmsg->header.chan == CHANNEL_A && tlmsg->header.opcode == A_GET_OPCODE)
        return sizeof(uint64_t) * 7;
    else if (tlmsg->header.chan == CHANNEL_A && tlmsg->header.opcode == A_PUTFULLDATA_OPCODE)
        return sizeof(uint64_t) * 4 + 2 << tlmsg->header.size;
}

void tloe_get_tlmsg(tloe_frame_t *frame, tl_msg_t *tlmsg, int loc) {
    int value_count = 0;
    int stop_flag = 0;
    int index = 0;
    
    // Calculate size
    for (int i=0; i<loc-1; i++) {
        for (int j=frame->flits[i]; j<frame->flits[i+1]; j++) {
            if (j == 0) {
                stop_flag = 1;
                break;
            }
        value_count++;
        if (stop_flag) break;
    }

    // Put data into tlmsg
    size_t total_size = sizeof(tl_msg_t) + (value_count * sizeof(uint64_t));
    tlmsg = (tl_msg_t *)realloc(tlmsg, total_size);
    BUG_ON(!tlmsg, "realloc failed");

    for (int i=0; i<loc-1; i++)
        for (int j=frame->flits[i]; j<frame->flits[i+1]; j++) {
            if (j == 0) {
                stop_flag = 1;
                break;
            }
            tlmsg->data[index++] = j;
        }
        if (stop_flag) break;
    }
#if 0
	smemcpy(tlmsg, &frame->flits[loc], sizeof(uint64_t));
#endif
}

void tloe_set_tlmsg(tloe_frame_t *frame, tl_msg_t *tlmsg, int loc) {
	memcpy(&frame->flits[loc], tlmsg, sizeof(uint64_t));
}

int is_zero_tl_frame(tloe_frame_t *frame, int size) {
#if 1
    uint64_t frame_mask = frame->flits[(size >> 3) - 1 - 1];

	return frame_mask == 0;
#else
    return 0;
#endif
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
