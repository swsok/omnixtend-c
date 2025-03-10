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
	return difftime(get_current_timestamp(ts), ref_time) >= TIMEOUT_TIME;
}

// Check if the half of the timeout has occurred
int is_timeout_tx_half(struct timespec *ts, time_t ref_time) {
	return difftime(get_current_timestamp(ts), ref_time) >= (TIMEOUT_TIME >> 1);
}

