#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "tloe_endpoint.h"
#include "tloe_ether.h"
#include "tloe_frame.h"
#include "tloe_transmitter.h"
#include "tloe_receiver.h"
#include "tilelink_msg.h"
#include "tilelink_handler.h"
#include "retransmission.h"
#include "timeout.h"
#include "util/circular_queue.h"
#include "util/util.h"

void init_tloe_endpoint(tloe_endpoint_t *e, TloeEther *ether, int master_slave) {
	e->is_done = 0;
	e->connection = 0;
	e->master = master_slave;

	e->next_tx_seq = 0;
	e->next_rx_seq = 0;
	e->acked_seq = MAX_SEQ_NUM;
	e->acked = 0;

    e->retransmit_buffer = create_queue(WINDOW_SIZE + 1);
    e->rx_buffer = create_queue(10); // credits
	e->message_buffer = create_queue(10000);
	e->ack_buffer = create_queue(100);
	e->tl_msg_buffer = create_queue(100);
	e->response_buffer = create_queue(100);

	e->ether = ether;

	init_timeout_rx(&(e->iteration_ts), &(e->timeout_rx));
	init_flowcontrol(&(e->fc));

	e->ack_cnt = 0;
	e->dup_cnt = 0;
	e->oos_cnt = 0;
	e->delay_cnt = 0;
	e->drop_cnt = 0;

	e->drop_npacket_size = 0;
	e->drop_npacket_cnt = 0;
	e->drop_apacket_size = 0;
	e->drop_apacket_cnt = 0;

	e->fc_inc_cnt = 0;
	e->fc_dec_cnt = 0;

	e->drop_tlmsg_cnt = 0;
	e->drop_response_cnt = 0;
}

void close_tloe_endpoint(tloe_endpoint_t *e) {
    // Join threads
    pthread_join(e->tloe_endpoint_thread, NULL);

    // Cleanup
    tloe_ether_close(e->ether);

    // Cleanup queues
    delete_queue(e->message_buffer);
    delete_queue(e->retransmit_buffer);
    delete_queue(e->rx_buffer);
    delete_queue(e->ack_buffer);
	delete_queue(e->tl_msg_buffer);
	delete_queue(e->response_buffer);
}

int is_conn(tloe_endpoint_t *e) {
	int result = 0;
	if (e->connection == 0)
		printf("Initiate the connection first.\n");
	else
		result = 1;

	return result;
}

// Select data to process from buffers based on priority order  
// ack_buffer -> response_buffer -> message_buffer
tl_msg_t *select_buffer(tloe_endpoint_t *e) {
	tl_msg_t *tlmsg = NULL;

	tlmsg = (tl_msg_t *) dequeue(e->response_buffer);
	if (tlmsg) 
		goto out;

	tlmsg = (tl_msg_t *) dequeue(e->message_buffer);
out:
	return tlmsg;
	// TODO free(tlmsg);
}

void *tloe_endpoint(void *arg) {
	tloe_endpoint_t *e = (tloe_endpoint_t *)arg;

	tl_msg_t *request_tlmsg = NULL;
	tl_msg_t *not_transmitted_tlmsg = NULL;

	while(!e->is_done) {
		if (!request_tlmsg)
			request_tlmsg = select_buffer(e);

		// Get reference timestemp for the single iteration
		update_iteration_timestamp(&(e->iteration_ts));

		not_transmitted_tlmsg = TX(e, request_tlmsg);
		if (not_transmitted_tlmsg) {
			request_tlmsg = not_transmitted_tlmsg;
			not_transmitted_tlmsg = NULL;
		} else if (!not_transmitted_tlmsg  && request_tlmsg) { 
			free(request_tlmsg);
			request_tlmsg = NULL;
		}

		RX(e);

		tl_handler(e);
	}
}

int main(int argc, char *argv[]) {
	tloe_endpoint_t *e;
	TloeEther *ether;
	char input, input_count[32];
	int master_slave = 0; 
	int iter = 0;

	if (argc < 3) {
		printf("Usage: tloe_endpoint queue_name master[1]/slave[0]\n");
		exit(0);
	}

	srand(time(NULL));

	master_slave = atoi(argv[2]);
	if (master_slave == 1)
		ether = tloe_ether_open(argv[1], 1);
	else
		ether = tloe_ether_open(argv[1], 0);

	e = (tloe_endpoint_t *)malloc(sizeof(tloe_endpoint_t));

	init_tloe_endpoint(e, ether, master_slave);

	if (pthread_create(&(e->tloe_endpoint_thread), NULL, tloe_endpoint, e) != 0) {
        error_exit("Failed to create tloe endpoint thread");
    }

	while(!(e->is_done)) {
		printf("Enter 'c' to open, 'd' to close, 's' to status, 'a' to send, 'q' to quit:\n");
		printf("> ");
		fgets(input_count, sizeof(input_count), stdin);

		if (sscanf(input_count, " %c %d", &input, &iter) < 1) {
			printf("Invalid input! Try again.\n");
			continue;
		}

		if (input == 's') {
			printf("-----------------------------------------------------\n");
			printf(" next_tx: %d, next_rx: %d, ack_cnt: %d, dup: %d, oos: %d, delay: %d, drop: %d, dnormal: %d, dack: %d\n", 
				e->next_tx_seq, e->next_rx_seq, e->ack_cnt, 
				e->dup_cnt, e->oos_cnt, e->delay_cnt, e->drop_cnt, 
				e->drop_npacket_cnt, e->drop_apacket_cnt);
			printf(" Credit: %d | %d | %d | %d | %d (%d/%d)\n", 
				e->fc.credits[CHANNEL_A], e->fc.credits[CHANNEL_B], e->fc.credits[CHANNEL_C], 
				e->fc.credits[CHANNEL_D], e->fc.credits[CHANNEL_E], e->fc_inc_cnt, e->fc_dec_cnt);
			printf(" Drop_tlmsg: %d, Drop_response: %d\n",
				e->drop_tlmsg_cnt, e->drop_response_cnt);
			printf("-----------------------------------------------------\n");
		} else if (input == 'a') {
			if (!is_conn(e)) continue;
			for (int i = 0; i < iter; i++) {
				tl_msg_t *new_tlmsg = (tl_msg_t *)malloc(sizeof(tl_msg_t));
				new_tlmsg->header.chan = CHANNEL_A;
				new_tlmsg->header.opcode = A_GET_OPCODE;
				// TODO new_tlmsg->num_flit = 4;
				if (!new_tlmsg) {
					printf("Memory allocation failed at packet %d!\n", i);
					continue;
				}

				while(is_queue_full(e->message_buffer)) 
					usleep(1000);

				if (enqueue(e->message_buffer, new_tlmsg)) {
					if (i % 100 == 0)
						fprintf(stderr, "Packet %d added to message_buffer\n", i);
				} else {
					//printf("Failed to enqueue packet %d, buffer is full.\n", i);
					free(new_tlmsg);
					break;  // Stop if buffer is full
				}
			}
		} else if (input == 'c') {
			// Connection
			if (!e->master) {
				printf("The connection should be initiated by the master.\n");
				continue;
			}

			open_conn(e);
			if (check_all_channels(&(e->fc))) {
				printf("Open connection is done. Credit %d | %d | %d | %d | %d\n",
					get_credit(&(e->fc), CHANNEL_A), get_credit(&(e->fc), CHANNEL_B),
					get_credit(&(e->fc), CHANNEL_C), get_credit(&(e->fc), CHANNEL_D),
					get_credit(&(e->fc), CHANNEL_E));
				e->connection = 1;
			}
		} else if (input == 'd') {
			// Disconnection
			init_tloe_endpoint(e, ether, master_slave);
		} else if (input == 'q') {
			e->is_done = 1;
			printf("Exiting...\n");
			break;
		} else {
			if (!is_conn(e)) continue;
		}

	}

	close_tloe_endpoint(e);

	return 0;
}

