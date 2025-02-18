#ifndef __TLOE_FRAME_H__
#define __TLOE_FRAME_H__
#include "tloe_ether.h"
#include "tilelink_msg.h"

#define TLOE_NAK        0
#define TLOE_ACK        1
#define MAX_SIZE_FLIT   65

/*
typedef struct tloe_frame_struct {
    // header
	int conn;
    int seq_num;
    int seq_num_ack;
    int ack; // ack = 1; nack = 0
    // TL messages 
	
	// Mask	
    int mask;
	int channel;
	int credit;

	tl_msg_t tlmsg;
} TloeFrame;
*/

// TLoE Frame Header 구조체
typedef struct {
    uint8_t vc:3;                  // Virtual Channel (3 bits)
    uint8_t type:4;                // Type (4 bits) : 1(Ack only) 2(Open Connection) 3(Close Connection)
    uint8_t reserved:3;            // Reserved (3 bits)
    uint32_t seq_num:22;           // Sequence Number (22 bits)
    uint32_t seq_num_ack:22;       // Sequence Number ACK (22 bits)
    uint8_t ack:1;                 // ACK (1 bit)
    uint8_t reserved2:1;           // Reserved bit
    uint8_t chan:3;                // Channel (3 bits)
    uint8_t credit:5;              // Credit (5 bits)
} tloe_header_t;

typedef struct __attribute__((packed,aligned(8))) {
    tloe_ether_header_t ether;
    tloe_header_t header;
    uint64_t flits[MAX_SIZE_FLIT];
} tloe_frame_t;

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

#endif // __TLOE_FRAME_H__
