#include "timeout.h"

// Update the reference timestamp for the single iteration
void update_iteration_timestamp(struct timespec *ts) {
	clock_gettime(CLOCK_MONOTONIC, ts);
}

// Get current timestamp in microseconds from the reference timestamp
long get_current_timestamp(struct timespec *ts) {
	return (ts->tv_sec * 1000000) + (ts->tv_nsec / 1000);
}

// Check if the timeout has occurred
int is_timeout_tx(struct timespec *ts, time_t ref_time) {
	return difftime(get_current_timestamp(ts), ref_time) >= DELAYED_ACK_TIME;
}

// Initialize the timeout receiver
void init_timeout_rx(struct timespec *ts, timeout_t *rx) {
	rx->ack_time = get_current_timestamp(ts);
	rx->last_ack_seq = -1;
	rx->ack_pending = 0;
	rx->ack_cnt = 0;
}

// Check if the delayed ACK should be sent
int is_send_delayed_ack(struct timespec *ts, timeout_t *rx) {
#if 0
	if (rx->ack_pending == 1)	
		return 1;
#else
	long curr_ms = get_current_timestamp(ts);
	long elapsed_ms = curr_ms - rx->ack_time;

	if (rx->ack_pending && elapsed_ms >= TIMEOUT_TIME) {
		return 1;
	}
	return 0;
#endif
}

