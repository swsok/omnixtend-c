#ifndef __TIMEOUT_H__
#define __TIMEOUT_H__
#include <time.h>

#define DELAYED_ACK_TIME 10000   // RTT * 2 (unit:us)
#define TIMEOUT_TIME     100      // RTT * 2 (unit:us)

typedef struct {
	long ack_time;
    int last_ack_seq;
    int ack_pending;
	int ack_cnt;
} timeout_t;

long get_current_time();
int is_timeout_tx(time_t);
void init_timeout_rx(timeout_t *);
int is_send_delayed_ack(timeout_t *);

#endif // __TIMEOUT_H__
