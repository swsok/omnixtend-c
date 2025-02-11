#ifndef __TIMEOUT_H__
#define __TIMEOUT_H__
#include <time.h>

#define TIMEOUT_TX 2   // RTT * 2
#define TIMEOUT_RX 1   // RTT * 2

typedef struct {
    time_t last_ack_time;
    int last_ack_seq;
    int ack_pending;
} TimeoutRX;

int is_timeout_tx(time_t send_time);
void init_timeout_rx(TimeoutRX *rx);
int is_send_delayed_ack(TimeoutRX *rx);

#endif // __TIMEOUT_H__
