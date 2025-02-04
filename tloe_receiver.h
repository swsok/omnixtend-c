#ifndef __TLOE_RECEIVER_H__
#define __TLOE_RECEIVER_H__
#include "tloe_ether.h"
#include "util/circular_queue.h"

extern CircularQueue *retransmit_buffer;
extern CircularQueue *ack_buffer;
extern int next_tx_seq;
extern int next_rx_seq;
extern int acked_seq;

void RX(TloeEther *ether);

#endif // __TLOE_RECEIVER_H__
