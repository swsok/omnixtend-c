#ifndef __TIMEOUT_H__
#define __TIMEOUT_H__
#include <time.h>

#define DELAYED_ACK_TIME 1   // RTT * 2 (unit:us)
#define TIMEOUT_TIME     100000   // RTT * 2 (unit:us)

typedef struct {
	long ack_time;
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

#endif // __TIMEOUT_H__
