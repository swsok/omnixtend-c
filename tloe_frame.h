#ifndef __TLOE_FRAME_H__
#define __TLOE_FRAME_H__
#include "tilelink_msg.h"

#define TLOE_NAK        0
#define TLOE_ACK        1

#define CHANNEL_NUM     6
#define CHANNEL_0       0
#define CHANNEL_A       1
#define CHANNEL_B       2
#define CHANNEL_C       3
#define CHANNEL_D       4
#define CHANNEL_E       5

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

	TileLinkMsg tlmsg;
} TloeFrame;

typedef struct tloe_buffer_struct {
	int ack;
	int seq_num_ack;
} TloeBuffer;

int tloe_set_seq_num(TloeFrame *frame, int seq_num);
int tloe_get_seq_num(TloeFrame *frame);
int tloe_set_seq_num_ack(TloeFrame *frame, int seq_num);
int tloe_get_seq_num_ack(TloeFrame *frame);
int tloe_set_ack(TloeFrame *frame, int ack);
int tloe_get_ack(TloeFrame *frame);
int tloe_set_mask(TloeFrame *frame, int mask);
int tloe_get_mask(TloeFrame *frame);
int is_ack_msg(TloeFrame *frame);
int is_conn_msg(TloeFrame *);

#endif // __TLOE_FRAME_H__
