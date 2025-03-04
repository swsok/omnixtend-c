#ifndef __RETRANSMISSION_H__
#define __RETRANSMISSION_H__

#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include "tloe_endpoint.h"
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
    int f_size;
    time_t send_time;
    tloe_frame_t tloe_frame;
} RetransmitBufferElement;

int retransmit(tloe_endpoint_t *, int);
void slide_window(tloe_endpoint_t *, tloe_frame_t *);
RetransmitBufferElement *get_earliest_element(CircularQueue *retransmit_buffer);

#endif // __RETRANSISSION_H__
