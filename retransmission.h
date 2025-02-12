#ifndef __RETRANSMISSION_H__
#define __RETRANSMISSION_H__

#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include "tloe_frame.h"
#include "tloe_ether.h"
#include "util/circular_queue.h"

#define TLOE_INIT       (1<<0)
#define TLOE_SENT       (1<<1)
#define TLOE_RESENT     (1<<2)
#define TLOE_ACKED      (1<<3)
#define TLOE_TIMEOUT    (1<<4)
#define WINDOW_SIZE     4

typedef struct {
    int state;
    time_t send_time;
    TloeFrame tloe_frame;
} RetransmitBufferElement;

int retransmit(TloeEther *ether, CircularQueue *retransmit_buffer, int seq_num_nack);
void slide_window(TloeEther *ether, CircularQueue *retransmit_buffer, int seq_num_ack);

#endif // __RETRANSISSION_H__
