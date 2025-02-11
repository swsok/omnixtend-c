#include "timeout.h"

int is_timeout_tx(time_t send_time) {
	return difftime(time(NULL), send_time) >= TIMEOUT_TX;
}

void init_timeout_rx(TimeoutRX *rx) {
    rx->last_ack_time = time(NULL);
    rx->last_ack_seq = -1;
    rx->ack_pending = 0;
}

int is_send_delayed_ack(TimeoutRX *rx) {
    time_t now = time(NULL);

    if (rx->ack_pending && (now - rx->last_ack_time) >= TIMEOUT_RX) {
        return 1;
    }
    return 0;
}

