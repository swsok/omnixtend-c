#ifndef __TIMEOUT_H__
#define __TIMEOUT_H__
#include <time.h>

#define DELAYED_ACK_TIME 1   // RTT * 2 (unit:us)
#define TIMEOUT_TIME     10000   // RTT * 2 (unit:us)

typedef struct {
	long ack_time;
	int last_ack_seq;
	int ack_pending;
	int last_channel;
	int last_credit;
	int ack_cnt;
} timeout_t;

// Update the reference time
void update_iteration_timestamp(struct timespec *);
// Get current time in microseconds from the reference time
long get_current_timestamp(struct timespec *);
// Check if the timeout has occurred
int is_timeout_tx(struct timespec *, time_t);
// Check if the half of the timeout has occurred
int is_timeout_tx_half(struct timespec *, time_t);
// Initialize the timeout receiver
void init_timeout_rx(struct timespec *, timeout_t *);
// Check if the delayed ACK should be sent
int is_send_delayed_ack(struct timespec *, timeout_t *);

#endif // __TIMEOUT_H__
