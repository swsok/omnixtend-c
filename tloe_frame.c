#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tloe_frame.h"

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

int is_zero_tl_frame(tloe_frame_t *frame) {
#if 1
	return (frame->flits[MAX_SIZE_FLIT-1] == 0);
#else
    return 0;
#endif
}

int is_conn_msg(tloe_frame_t *frame) {
    return (frame->header.type == 2 || frame->header.type == 3) ? frame->header.type : -1;
}

void tloe_frame_to_packet(tloe_frame_t *tloeframe, char *send_buffer, int send_buffer_size) {
	uint64_t be64_temp;
	int offset = 0;

	bzero((void*)send_buffer, send_buffer_size);

	// TLoE frame Header
	be64_temp = htobe64(*(uint64_t*)&(tloeframe->header));
	memcpy(send_buffer+offset, &be64_temp, sizeof(uint64_t));
	offset += sizeof(uint64_t);

	memcpy(send_buffer+offset, tloeframe->flits, sizeof(uint64_t) * MAX_SIZE_FLIT);
}

void packet_to_tloe_frame(char *recv_buffer, int recv_buffer_size, tloe_frame_t *tloeframe) {
    uint64_t be64_temp;
    int offset = 0;

    // 패킷 크기 검증
    if (recv_buffer_size < sizeof(uint64_t) + (sizeof(uint64_t) * MAX_SIZE_FLIT)) {
        fprintf(stderr, "Received packet size is too small!\n");
        return;
    }

    // TLoE Frame Header 변환 (네트워크 바이트 순서를 호스트 바이트 순서로 변환)
    memcpy(&be64_temp, recv_buffer + offset, sizeof(uint64_t)); // 버퍼에서 8바이트 복사
    be64_temp = be64toh(be64_temp);  // Big-endian → Host-endian 변환
    memcpy(&(tloeframe->header), &be64_temp, sizeof(tloe_header_t)); // 변환된 값 구조체에 복사
    offset += sizeof(uint64_t);

    // Flits 데이터 복사
    memcpy(tloeframe->flits, recv_buffer + offset, sizeof(uint64_t) * MAX_SIZE_FLIT);
}
#if 0
void tloe_frame_to_packet(tloe_frame_t *tloeframe, char *send_buffer, int send_buffer_size) {
	uint64_t be64_temp;
	int offset = 0;

	bzero((void*)send_buffer, send_buffer_size);

	// TLoE frame Header
	be64_temp = htobe64(*(uint64_t*)&(tloeframe->header));
	memcpy(send_buffer+offset, &be64_temp, sizeof(uint64_t));
	offset += sizeof(uint64_t);

	memcpy(send_buffer+offset, tloeframe->flits, sizeof(uint64_t) * MAX_SIZE_FLIT);
}

void packet_to_tloe_frame(char *recv_buffer, int recv_buffer_size, tloe_frame_t *tloeframe) {
    uint64_t be64_temp;
    int offset = 0;

    // 패킷 크기 검증
    if (recv_buffer_size < sizeof(uint64_t) + (sizeof(uint64_t) * MAX_SIZE_FLIT)) {
        fprintf(stderr, "Received packet size is too small!\n");
        return;
    }

    // TLoE Frame Header 변환 (네트워크 바이트 순서를 호스트 바이트 순서로 변환)
    memcpy(&be64_temp, recv_buffer + offset, sizeof(uint64_t)); // 버퍼에서 8바이트 복사
    be64_temp = be64toh(be64_temp);  // Big-endian → Host-endian 변환
    memcpy(&(tloeframe->header), &be64_temp, sizeof(tloe_header_t)); // 변환된 값 구조체에 복사
    offset += sizeof(uint64_t);

    // Flits 데이터 복사
    memcpy(tloeframe->flits, recv_buffer + offset, sizeof(uint64_t) * MAX_SIZE_FLIT);
}
#endif
