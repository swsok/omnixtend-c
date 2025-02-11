#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include "tloe_ether.h"
#include "tloe_frame.h"
#include "tloe_transmitter.h"
#include "tloe_receiver.h"
#include "tilelink_msg.h"
#include "retransmission.h"
#include "util/circular_queue.h"
#include "util/util.h"

#define MAX_SEQ_NUM		((1<<10)-1)
		      
CircularQueue *retransmit_buffer;
CircularQueue *rx_buffer;
CircularQueue *message_buffer;
CircularQueue *ack_buffer;

int next_tx_seq = 0;
int next_rx_seq = 0;
int acked_seq = MAX_SEQ_NUM;
int acked = 0;

static TloeEther *ether;
static int is_done = 1;
int test_timeout = 0;  // for test timeout

#if 0
time_t last_ack_time = 0;
#endif

void *tloe_endpoint(void *arg) {
	TloeFrame *request_tloeframe = NULL;
	TloeFrame *not_transmitted_frame = NULL;

	while(is_done) {
		if (!request_tloeframe && !is_queue_empty(message_buffer)) 
			request_tloeframe = dequeue(message_buffer);

		not_transmitted_frame = TX(request_tloeframe, ether);
		if (not_transmitted_frame) {
			request_tloeframe = not_transmitted_frame;
			not_transmitted_frame = NULL;
		} else if (!not_transmitted_frame  && request_tloeframe) { 
			free(request_tloeframe);
			request_tloeframe = NULL;
		}

		RX(ether);
	}
}

int main(int argc, char *argv[]) {
    pthread_t tloe_endpoint_thread;
    int master_slave = 1;
	char input, input_count[32];
	int iter = 0;

    if (argc < 3) {
        printf("Usage: tloe_endpoint queue_name master[1]/slave[0]\n");
        exit(0);
    }

    srand(time(NULL));

    if (argv[2][0] == '1')
        ether = tloe_ether_open(argv[1], 1);
    else
        ether = tloe_ether_open(argv[1], 0);

    retransmit_buffer = create_queue(WINDOW_SIZE + 1);
    rx_buffer = create_queue(10); // credits
	message_buffer = create_queue(10000);
	ack_buffer = create_queue(100);

	if (pthread_create(&tloe_endpoint_thread, NULL, tloe_endpoint, NULL) != 0) {
        error_exit("Failed to ack reply thread");
    }
    
	while(1) {
		printf("Enter 's' to status, 'a' to send, 'q' to quit:\n");
		printf("> ");
		fgets(input_count, sizeof(input_count), stdin);

		if (sscanf(input_count, " %c %d", &input, &iter) < 1) {
			printf("Invalid input! Try again.\n");
			continue;
		}

		if (input == 's') {
			printf("-----------------------------------------------------\n");
			printf(" next_tx_seq: %d, acked_seq: %d, next_rx_seq: %d\n", next_tx_seq, acked_seq, next_rx_seq);
			printf("-----------------------------------------------------\n");
		} else if (input == 'a') {
			for (int i = 0; i < iter; i++) {
				TloeFrame *new_tloe = (TloeFrame *)malloc(sizeof(TloeFrame));
				if (!new_tloe) {
					printf("Memory allocation failed at packet %d!\n", i);
					continue;
				}

				new_tloe->mask = 1;  // Set mask (1 = normal packet)

				while(is_queue_full(message_buffer)) 
					usleep(1000);

				if (enqueue(message_buffer, new_tloe)) {
					if (i % 100 == 0)
						fprintf(stderr, "Packet %d added to message_buffer\n", i);
				} else {
					//printf("Failed to enqueue packet %d, buffer is full.\n", i);
					free(new_tloe);
					break;  // Stop if buffer is full
				}
			}
		} else if (input == 'i') {
			// Test timeout
			test_timeout = iter;
		} else if (input == 'q') {
			printf("Exiting...\n");
			break;
		}
	}

	is_done = 1;

    // Join threads
    pthread_join(tloe_endpoint_thread, NULL);

    // Cleanup
    tloe_ether_close(ether);

    // Cleanup queues
    delete_queue(message_buffer);
    delete_queue(retransmit_buffer);
    delete_queue(rx_buffer);
    delete_queue(ack_buffer);

    return 0;
}

