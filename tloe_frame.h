#ifndef __TLOE_FRAME_H__
#define __TLOE_FRAME_H__
#include "tloe_ether.h"
#include "tilelink_msg.h"

#define TLOE_NAK              0
#define TLOE_ACK              1
#define TYPE_ACKONLY          1
#define TYPE_OPEN_CONNECTION  2
#define TYPE_CLOSE_CONNECTION 3
#define MAX_SIZE_FLIT         65

typedef struct __attribute__((packed,aligned(8))) {
    unsigned char credit:5;
    unsigned char chan:3;
    unsigned char reserve3:1;
    unsigned char ack:1;
    unsigned int seq_num_ack:22;
    unsigned int seq_num:22;
    unsigned char reserve2:2;
    unsigned char reserve1:1;
    unsigned char type:4;
    unsigned char vc:3;
} tloe_header_t;

typedef struct __attribute__((packed,aligned(8))) {
    tloe_header_t header;
    uint64_t flits[MAX_SIZE_FLIT];
} tloe_frame_t;

void set_tloe_frame(tloe_frame_t *, tl_msg_t *, uint32_t, uint32_t, uint8_t, uint8_t, uint8_t);
int tloe_set_seq_num(tloe_frame_t *, int);
int tloe_get_seq_num(tloe_frame_t *);
int tloe_set_seq_num_ack(tloe_frame_t *, int);
int tloe_get_seq_num_ack(tloe_frame_t *);
int tloe_set_ack(tloe_frame_t *, int);
int tloe_get_ack(tloe_frame_t *);
int tloe_set_mask(tloe_frame_t *, int);
int tloe_get_mask(tloe_frame_t *);
void tloe_get_tlmsg(tloe_frame_t *, tl_msg_t *, int);
void tloe_set_tlmsg(tloe_frame_t *, tl_msg_t *, int);
int is_ack_msg(tloe_frame_t *);
int is_conn_msg(tloe_frame_t *);
void tloe_frame_to_packet(tloe_frame_t *, char *, int);
void packet_to_tloe_frame(char *, int, tloe_frame_t *);

#endif // __TLOE_FRAME_H__
