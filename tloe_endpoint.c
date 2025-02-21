#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <getopt.h>

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

#define CHECK_FABRIC_TYPE_CONFLICT(current_type, expected_type, msg) do { \
    if (current_type != -1 && current_type != expected_type) { \
        fprintf(stdout, "Error: %s\n", msg); \
        print_usage(); \
        ret = -1; \
        goto out; \
    } \
} while(0)

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

static void print_usage(void) {
    fprintf(stdout, "Usage: tloe_endpoint {-i<ethernet interface> -d<destination mac address> | -p<mq name>} {-m | -s}\n");
}

static int parse_arguments(int argc, char *argv[], int *fabric_type, int *master, 
                         char *optarg_a, size_t optarg_a_size,
                         char *optarg_b, size_t optarg_b_size) {
    int opt;
    int ret = 0;
    *fabric_type = -1;
    *master = -1;

    while ((opt = getopt(argc, argv, "i:d:p:ms")) != -1) {
        switch (opt) {
            case 'i':
                CHECK_FABRIC_TYPE_CONFLICT(*fabric_type, TLOE_FABRIC_ETHER, 
                    "Cannot mix ethernet and mq mode options");
                *fabric_type = TLOE_FABRIC_ETHER;
                strncpy(optarg_a, optarg, optarg_a_size - 1);
                optarg_a[optarg_a_size - 1] = '\0';
                break;
            case 'd':
                CHECK_FABRIC_TYPE_CONFLICT(*fabric_type, TLOE_FABRIC_ETHER,
                    "Cannot mix ethernet and mq mode options");
                *fabric_type = TLOE_FABRIC_ETHER;
                strncpy(optarg_b, optarg, optarg_b_size - 1);
                optarg_b[optarg_b_size - 1] = '\0';
                break;
            case 'p':
                CHECK_FABRIC_TYPE_CONFLICT(*fabric_type, TLOE_FABRIC_MQ,
                    "Cannot mix ethernet and mq mode options");
                *fabric_type = TLOE_FABRIC_MQ;
                strncpy(optarg_a, optarg, optarg_a_size - 1);
                optarg_a[optarg_a_size - 1] = '\0';
                break;
            case 'm':
                *master = 1;
                break;
            case 's':
                *master = 0;
                break;
            default:
                print_usage();
                ret = -1;
                goto out;
        }
    }

    // fabric type must be specified
    if (*fabric_type == -1) {
        fprintf(stdout, "Error: Must specify either ethernet mode (-i, -d) or mq mode (-p)\n");
        print_usage();
        ret = -1;
        goto out;
    }

    // ethernet mode requires both interface and destination MAC	
    if (*fabric_type == TLOE_FABRIC_ETHER && (optarg_a[0] == '\0' || optarg_b[0] == '\0')) {
        fprintf(stdout, "Error: Ethernet mode requires both interface (-i) and destination MAC (-d)\n");
        print_usage();
        ret = -1;
        goto out;
    }

    // master or slave mode must be specified
    if (*master == -1) {
        fprintf(stdout, "Error: Must specify either master (-m) or slave (-s) mode\n");
        print_usage();
        ret = -1;
        goto out;
    }

    if (*fabric_type == TLOE_FABRIC_MQ) {
        strncpy(optarg_b, *master ? "-master" : "-slave", sizeof("-master"));
    }

out:
    return ret;
}

int main(int argc, char *argv[]) {
    tloe_endpoint_t *e;
    TloeEther *ether;
    char input, input_count[32];
    int master_slave;
    int iter = 0;
    char dev_name[64] = {0};
    char dest_mac_addr[64] = {0};
    int fabric_type;

    if (parse_arguments(argc, argv, &fabric_type, &master_slave, 
                       dev_name, sizeof(dev_name),
                       dest_mac_addr, sizeof(dest_mac_addr)) < 0) {
        exit(EXIT_FAILURE);
    }

#if defined(DEBUG) || defined(TEST_NORMAL_FRAME_DROP) || defined(TEST_TIMEOUT_DROP)
    srand(time(NULL));
#endif

    // initialization independent of tloe_endpoint
    tl_handler_init();

    // Initialize tloe_endpoint
    e = (tloe_endpoint_t *)malloc(sizeof(tloe_endpoint_t));
    tloe_endpoint_init(e, fabric_type, master_slave);
    tloe_fabric_init(e, fabric_type);

    // intead of a direct call to tloe_ether_open
    tloe_fabric_open(e, dev_name, dest_mac_addr);

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

