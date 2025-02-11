#ifndef __TIMEOUT_H__
#define __TIMEOUT_H__
#include <time.h>

#define TIMEOUT_TX 10000   // RTT * 2 (unit:us)
#define TIMEOUT_RX 100     // RTT * 2 (unit:us)

typedef struct {
	long last_ack_time;
    int last_ack_seq;
    int ack_pending;
} TimeoutRX;

long get_current_time();
int is_timeout_tx(time_t send_time);
void init_timeout_rx(TimeoutRX *rx);
int is_send_delayed_ack(TimeoutRX *rx);

#endif // __TIMEOUT_H__
