#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "tloe_receiver.h"
#include "tloe_frame.h"
#include "tloe_endpoint.h"
#include "tloe_common.h"
#include "retransmission.h"
#include "timeout.h"

static int ack_cnt = 0;
static int rx_count = 0;
static TimeoutRX timeout_rx;

extern int test_timeout;
int test_timeout_tx = 10;
int drop_ack = 0;

void RX(TloeEther *ether) {
    int size;
    TloeFrame *tloeframe = (TloeFrame *)malloc(sizeof(TloeFrame));

	// Timeout RX
	static int initialized = 0;
	if(!initialized) {
		init_timeout_rx(&timeout_rx);
		initialized = 1;
	}

	if (is_send_delayed_ack(&timeout_rx)) {
#if 1  // (Test) Timeout TX: drop ack
        if (!drop_ack && ((rand() % 100) == 1)) {
			drop_ack = 1;	
			test_timeout_tx = 10;
		}
		if (drop_ack && (test_timeout_tx > 0)) {
			test_timeout_tx -= 1;
			if (test_timeout_tx == 0) drop_ack = 0; 
		} else {
#endif
		TloeFrame *frame = malloc(sizeof(TloeFrame));

	    *frame = *tloeframe;
		frame->seq_num = timeout_rx.last_ack_seq;
		frame->seq_num_ack = timeout_rx.last_ack_seq;
		frame->ack = TLOE_ACK;
		frame->mask = 0;

		if (!enqueue(ack_buffer, (void *) frame)) {
			printf("File: %s line: %d: enqueue error\n", __FILE__, __LINE__);
			exit(1);
		}
		timeout_rx.ack_pending = 0;
#if 1
		}
#endif
	}

    // Receive a frame from the Ethernet layer
    size = tloe_ether_recv(ether, (char *)tloeframe);
    if (size == -1 && errno == EAGAIN) {
        // No data available, return without processing
        //printf("File: %s line: %d: tloe_ether_recv error\n", __FILE__, __LINE__);
        free(tloeframe);
        return;
    }

#if 1 // (Test) Timout TX: drop normal packet
{
	if (test_timeout-- > 0) {
		printf("Drop packet of seq_num %d\n", tloeframe->seq_num);
		free(tloeframe);
		return;
	}
}
#endif

	// ACK/NAK (zero-TileLink)
    if (is_ack_msg(tloeframe)) {
        slide_window(ether, retransmit_buffer, tloeframe->seq_num_ack);

        // Handle ACK/NAK and finish processing
        if (tloeframe->ack == TLOE_NAK)  // NAK
            retransmit(ether, retransmit_buffer, tloeframe->seq_num_ack);

        // Update sequence numbers
        // Note that next_rx_seq must not be increased
	    acked_seq = tloeframe->seq_num_ack;
	    ack_cnt++;

        free(tloeframe);

        if (ack_cnt % 100 == 0) {
            fprintf(stderr, "next_tx: %d, ackd: %d, next_rx: %d, ack_cnt: %d\n",
                next_tx_seq, acked_seq, next_rx_seq, ack_cnt);
        }
        return;
    }

    //printf("RX: Received packet with seq_num: %d, seq_num_ack: %d, ack: %d\n", 
    //    tloeframe->seq_num, tloeframe->seq_num_ack, tloeframe->ack);

    int diff_seq = seq_num_diff(tloeframe->seq_num, next_rx_seq);

    // If the received frame has the expected sequence number
    if (diff_seq == 0) {
        if (is_ack_msg(tloeframe)) {
        fprintf(stderr, "Error: invalid message\n");
        exit(1);
	}

#if 1 // test: Packet drop
    if (1) {
        extern int master_slave;
        if ((rand() % 10000) == 99) {
            free(tloeframe);
            return;
        }
    }
#endif

    // Normal request packet
    // Handle and enqueue it into the message buffer

	// Handle TileLink Msg
	// TODO
	//printf("RX: Send pakcet to Tx channel for replying ACK/NAK with seq_num: %d, seq_num_ack: %d, ack: %d\n",
	//    tloeframe->seq_num, tloeframe->seq_num_ack, tloeframe->ack);

	// Timeout RX
	timeout_rx.last_ack_seq = tloeframe->seq_num;
	timeout_rx.ack_pending = 1;
	timeout_rx.last_ack_time = get_current_time();

    // Update sequence numbers
   	next_rx_seq = (tloeframe->seq_num + 1) % (MAX_SEQ_NUM+1);
    acked_seq = tloeframe->seq_num_ack;

    // tloeframe must be freed here
    free(tloeframe);

    //if (next_tx_seq % 100 == 0)
    //fprintf(stderr, "next_tx: %d, ackd: %d, next_rx: %d, ack_cnt: %d\n", 
    //    next_tx_seq, acked_seq, next_rx_seq, ack_cnt);
    } else if (diff_seq < (MAX_SEQ_NUM + 1) / 2) {
        // The received TLoE frame is a duplicate
        // The frame should be dropped, NEXT_RX_SEQ is not updated
        // A positive acknowledgment is sent using the received sequence number
        int seq_num = tloeframe->seq_num;
        printf("TLoE frame is a duplicate. seq_num: %d, next_rx_seq: %d\n", seq_num, next_rx_seq);

        // If the received frame contains data, enqueue it in the message buffer
        if (is_ack_msg(tloeframe)) {
            printf("File: %s line: %d: duplication error\n", __FILE__, __LINE__);
            exit(1);
            free(tloeframe);
        }

        tloeframe->seq_num_ack = seq_num;
        tloeframe->ack = TLOE_ACK;
        tloeframe->mask = 0; // To indicate ACK
        if (!enqueue(ack_buffer, (void *) tloeframe)) {
            printf("File: %s line: %d: enqueue error\n", __FILE__, __LINE__);
            exit(1);
        }
		init_timeout_rx(&timeout_rx);
    } else {
        // The received TLoE frame is out of sequence, indicating that some frames were lost
        // The frame should be dropped, NEXT_RX_SEQ is not updated
        // A negative acknowledgment (NACK) is sent using the last properly received sequence number
        int last_proper_rx_seq = (next_rx_seq - 1) % (MAX_SEQ_NUM+1);
        if (last_proper_rx_seq < 0)
            last_proper_rx_seq += MAX_SEQ_NUM + 1;

        printf("TLoE frame is out of sequence with ");
		printf("seq_num: %d, next_rx_seq: %d last: %d\n",
			tloeframe->seq_num, next_rx_seq, last_proper_rx_seq);

        // If the received frame contains data, enqueue it in the message buffer
		if (is_ack_msg(tloeframe)) {
			printf("File: %s line: %d: out of sequence error\n", __FILE__, __LINE__);
			exit(1);
			free(tloeframe);
		}

		tloeframe->seq_num_ack = last_proper_rx_seq;
		tloeframe->ack = TLOE_NAK;  // NAK
		tloeframe->mask = 0; // To indicate ACK
		if (!enqueue(ack_buffer, (void *) tloeframe)) {
			printf("File: %s line: %d: enqueue error\n", __FILE__, __LINE__);
			exit(1);
		}
		init_timeout_rx(&timeout_rx);
	}
}

