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

static void tloe_endpoint_init(tloe_endpoint_t *e, int fabric_type, int master_slave) {
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

static void tloe_endpoint_close(tloe_endpoint_t *e) {
    // Join threads
    pthread_join(e->tloe_endpoint_thread, NULL);

    // Cleanup
    tloe_fabric_close(e);

    // Cleanup
    tl_handler_close();

    // Cleanup queues
    delete_queue(e->message_buffer);
    delete_queue(e->retransmit_buffer);
    delete_queue(e->rx_buffer);
    delete_queue(e->ack_buffer);
	delete_queue(e->tl_msg_buffer);
	delete_queue(e->response_buffer);
}

static int is_conn(tloe_endpoint_t *e) {
	int result = 0;
	if (e->connection == 0)
		printf("Initiate the connection first.\n");
	else
		result = 1;

	return result;
}

// Select data to process from buffers based on priority order  
// ack_buffer -> response_buffer -> message_buffer
static tl_msg_t *select_buffer(tloe_endpoint_t *e) {
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

static void print_endpoint_status(tloe_endpoint_t *e) {
    printf("-----------------------------------------------------\n");
    printf("Sequence Numbers:\n");
    printf(" TX: %d, RX: %d\n", e->next_tx_seq, e->next_rx_seq);
    
    printf("\nPacket Statistics:\n");
    printf(" ACK: %d, Duplicate: %d, Out-of-Sequence: %d\n", 
           e->ack_cnt, e->dup_cnt, e->oos_cnt);
    printf(" Delayed: %d, Dropped: %d (Normal: %d, ACK: %d)\n",
           e->delay_cnt, e->drop_cnt, e->drop_npacket_cnt, e->drop_apacket_cnt);
    
    printf("\nChannel Credits [A|B|C|D|E]: %d|%d|%d|%d|%d\n",
           e->fc.credits[CHANNEL_A], e->fc.credits[CHANNEL_B], 
           e->fc.credits[CHANNEL_C], e->fc.credits[CHANNEL_D], 
           e->fc.credits[CHANNEL_E]);
    printf(" Flow Control (Inc/Dec): %d/%d\n", e->fc_inc_cnt, e->fc_dec_cnt);
    
    printf("\nBuffer Drops:\n");
    printf(" TL Messages: %d, Responses: %d\n", 
           e->drop_tlmsg_cnt, e->drop_response_cnt);
    printf("-----------------------------------------------------\n");
}

static int create_and_enqueue_message(tloe_endpoint_t *e, int msg_index) {
    tl_msg_t *new_tlmsg = (tl_msg_t *)malloc(sizeof(tl_msg_t));
    if (!new_tlmsg) {
        printf("Memory allocation failed at packet %d!\n", msg_index);
        return 0;
    }

    new_tlmsg->header.chan = CHANNEL_A;
    new_tlmsg->header.opcode = A_GET_OPCODE;
    // TODO new_tlmsg->num_flit = 4;

    while(is_queue_full(e->message_buffer)) 
        usleep(1000);

    if (enqueue(e->message_buffer, new_tlmsg)) {
        if (msg_index % 100 == 0)
            fprintf(stderr, "Packet %d added to message_buffer\n", msg_index);
        return 1;
    } else {
        free(new_tlmsg);
        return 0;
    }
}

static void print_credit_status(tloe_endpoint_t *e) {
    printf("Open connection is done. Credit %d | %d | %d | %d | %d\n",
        get_credit(&(e->fc), CHANNEL_A), get_credit(&(e->fc), CHANNEL_B),
        get_credit(&(e->fc), CHANNEL_C), get_credit(&(e->fc), CHANNEL_D),
        get_credit(&(e->fc), CHANNEL_E));
}

static int handle_user_input(tloe_endpoint_t *e, char input, int iter, 
				int fabric_type, int master) {
    int ret = 0;

    if (input == 's') {
        print_endpoint_status(e);
    } else if (input == 'a') {
        if (!is_conn(e)) return 0;
        for (int i = 0; i < iter; i++) {
            if (!create_and_enqueue_message(e, i)) {
                break;  // Stop if buffer is full or allocation fails
            }
        }
    } else if (input == 'c') {
        // Connection
        if (!e->master) {
            printf("The connection should be initiated by the master.\n");
            goto out;
        }

        open_conn(e);
        if (check_all_channels(&(e->fc))) {
            print_credit_status(e);
            e->connection = 1;
        }
    } else if (input == 'd') {
        // Disconnection
        printf("disconnection not implemented yet\n");
        tloe_endpoint_init(e, fabric_type, master);
    } else if (input == 'q') {
        e->is_done = 1;
        printf("Exiting...\n");
        ret = 1;
        goto out;
    } else {
        if (!is_conn(e)) return 0;
    }
out:
    return ret;
}

int main(int argc, char *argv[]) {
	tloe_endpoint_t *e;
	TloeEther *ether;
	char input, input_count[32];
	int master_slave = 0; 
	int iter = 0;
	char dev_name[64];
	char ip_addr[64];
	int fabric_type = TLOE_FABRIC_ETHER;

	if (argc < 4) {
		printf("Usage: tloe_endpoint eth-if dest_mac master[1]/slave[0]\n");
		exit(EXIT_FAILURE);
	}

	strncpy(dev_name, argv[1], 64);
	master_slave = atoi(argv[3]);
	switch (fabric_type) {
		case TLOE_FABRIC_ETHER:
			strncpy(ip_addr, argv[2], 64);
			break;
		case TLOE_FABRIC_MQ:
			strncpy(ip_addr, master_slave ? "-a" : "-b", sizeof("-a"));
		default:
			break;
	}

#if defined(DEBUG) || defined(TEST_NORMAL_FRAME_DROP) || defined(TEST_TIMEOUT_DROP)
	srand(time(NULL));
#endif

    // Initialize
	e = (tloe_endpoint_t *)malloc(sizeof(tloe_endpoint_t));
	tloe_endpoint_init(e, fabric_type, master_slave);
	tloe_fabric_init(e, fabric_type);
    tl_handler_init();

	// intead of a direct call to tloe_ether_open
	tloe_fabric_open(e, dev_name, ip_addr);

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

		if (handle_user_input(e, input, iter, fabric_type, master_slave))
			break;
	}

	tloe_endpoint_close(e);

	return 0;
}

