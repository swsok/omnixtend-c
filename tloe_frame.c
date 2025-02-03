#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tloe_frame.h"

int tloe_set_seq_num(TloeFrame *frame, int seq_num) {
    frame->seq_num = seq_num;
    return seq_num;
}

int tloe_get_seq_num(TloeFrame *frame) {
    return frame->seq_num;
}

int tloe_set_seq_num_ack(TloeFrame *frame, int seq_num) {
    frame->seq_num_ack = seq_num;
    return seq_num;
}

int tloe_get_seq_num_ack(TloeFrame *frame) {
    return frame->seq_num_ack;
}

int tloe_set_ack(TloeFrame *frame, int ack) {
    frame->ack = ack;
    return ack;
}

int tloe_get_ack(TloeFrame *frame) {
    return frame->ack;
}
