#ifndef __TLOE_FRAME_H__
#define __TLOE_FRAME_H__

#define TLOE_NACK       0
#define TLOE_ACK        1

typedef struct tloe_frame_struct {
    // header
    int seq_num;
    int seq_num_ack;
    int ack; // ack = 1; nack = 0
    // TLOE messages 
} TloeFrame;

int tloe_set_seq_num(TloeFrame *frame, int seq_num);
int tloe_get_seq_num(TloeFrame *frame);
int tloe_set_seq_num_ack(TloeFrame *frame, int seq_num);
int tloe_get_seq_num_ack(TloeFrame *frame);
int tloe_set_ack(TloeFrame *frame, int ack);
int tloe_get_ack(TloeFrame *frame);

#endif // __TLOE_FRAME_H__
