#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tloe_frame.h"
#include "tloe_common.h"
#include "tilelink_msg.h"

int tloe_set_ack(tloe_frame_t *frame, int ack) {
    frame->header.ack = ack;
    return ack;
}

int tloe_get_ack(tloe_frame_t *frame) {
    return frame->header.ack;
}

// Convert packet_size to flit_cnt (>>3 - 1) and decrease tloe_header by 1
uint64_t tloe_set_mask(tloe_frame_t *frame, int mask, int packet_size) {
    frame->flits[((packet_size >> 3) - 1) - (sizeof(uint64_t) / 8)] = mask;
    return mask;
}

// Convert packet_size to flit_cnt (>>3 - 1) and decrease tloe_header by 1
uint64_t tloe_get_mask(tloe_frame_t *frame, int packet_size) {
    return frame->flits[((packet_size >> 3) - 1) - (sizeof(uint64_t) / 8)];
}

int tloe_get_fsize(tl_msg_t *tlmsg) {
    if (tlmsg == NULL) return DEFAULT_FRAME_SIZE;

    int fsize = sizeof(uint64_t) + tlmsg_get_total_size(tlmsg);

    // If the frame size is smaller than the minimum Ethernet packet size, set it to the minimum size.  
    return (fsize < DEFAULT_FRAME_SIZE) ? DEFAULT_FRAME_SIZE : fsize;
}

tl_msg_t *tloe_get_tlmsg(tloe_frame_t *frame, int tl_loc) {
    int data_size;
    tl_msg_t tl_msg_header;
    tl_msg_t *tlmsg;

    tl_header_t *tlheader = (tl_header_t*) &frame->flits[tl_loc];
    tl_msg_header.header.chan = tlheader->chan;
    tl_msg_header.header.opcode = tlheader->opcode;
    tl_msg_header.header.size = tlheader->size;

    // Calculate the total size of tlmsg in bytes.  
    size_t total_size = tlmsg_get_total_size(&tl_msg_header); 

    tlmsg = (tl_msg_t *)malloc(total_size);
    BUG_ON(!tlmsg, "malloc failed");

    memcpy(tlmsg, &frame->flits[tl_loc], total_size);

    return tlmsg;
}

void tloe_set_tlmsg(tloe_frame_t *frame, tl_msg_t *tlmsg) {
    size_t tl_size = tlmsg_get_total_size(tlmsg);

    memcpy(&frame->flits[0], tlmsg, tl_size);
}

int is_zero_tl_frame(tloe_frame_t *frame, int size) {
    //uint64_t frame_mask = frame->flits[(size >> 3) - 1 - 1];
    uint64_t frame_mask = tloe_get_mask(frame, size);

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

    // Flits
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

    // Flits
    for (int i = 0; i < num_flits; i++) {
        memcpy(&be64_temp, recv_buffer + offset, sizeof(uint64_t));
        be64_temp = be64toh(be64_temp);  // big-endian에서 호스트 순서로 변환
        tloeframe->flits[i] = be64_temp;
        offset += sizeof(uint64_t);
    }

}
