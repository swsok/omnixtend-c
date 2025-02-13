#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "tloe_receiver.h"
#include "tloe_frame.h"
#include "tloe_endpoint.h"
#include "tloe_common.h"
#include "retransmission.h"
#include "timeout.h"

static void serve_conn(tloe_endpoint_t *e, TloeFrame *recv_tloeframe) {
	// Set credits
	set_credit(&(e->fc), recv_tloeframe->channel, recv_tloeframe->credit);

	// If slave, send chan/credit message
	if (e->master == 0) {
		TloeFrame *f = (TloeFrame *)malloc(sizeof(TloeFrame));

		f->conn = 1;
		// Update the sequence number
		f->seq_num = e->next_tx_seq;
		// Update the sequence number of the ack
		f->seq_num_ack = e->acked_seq;
		// Set the ack to TLOE_ACK
		f->ack = TLOE_ACK;
		// Set the mask to indicate normal packet
		f->mask = 0;
		// Set 0 to channel and credit
		f->channel = recv_tloeframe->channel;
		f->credit = CREDIT_DEFAULT;
		// Send the request_normal_frame using the ether
		tloe_ether_send(e->ether, (char *)f, sizeof(TloeFrame));
		// increase the sequence number of the endpoint
		e->next_tx_seq = tloe_seqnum_next(e->next_tx_seq);
		// Free TloeFrame
		free(f);

		init_timeout_rx(&(e->iteration_ts), &(e->timeout_rx));

		// Check whether all channel/credit received
		if (e->connection == 0) {
			int check_con = 0;
			for (int i=1; i < CHANNEL_NUM; i++) {
				if (get_credit(&(e->fc), i) <= 128)
					check_con = 1;
			}

			if (!check_con) {
				printf("Open connection is done. Credit %d | %d | %d | %d | %d\n",
						get_credit(&(e->fc), CHANNEL_A), get_credit(&(e->fc), CHANNEL_B),
						get_credit(&(e->fc), CHANNEL_C), get_credit(&(e->fc), CHANNEL_D),
						get_credit(&(e->fc), CHANNEL_E));
				e->connection = 1;
			}
		}
	}
	// Update sequence numbers
	e->next_rx_seq = tloe_seqnum_next(recv_tloeframe->seq_num);
	e->acked_seq = recv_tloeframe->seq_num_ack;
}

static void serve_ack(tloe_endpoint_t *e, TloeFrame *recv_tloeframe) {
	// Slide retransmission buffer for flushing ancester frames
	// Note that ACK/NAK transmit the sequence number of the received frame as seq_num_ack
	slide_window(e, recv_tloeframe->seq_num_ack);

	// Additionally, NAK re-transmit the frames in the retransmission buffer
	if (recv_tloeframe->ack == TLOE_NAK)  // NAK
		retransmit(e, tloe_seqnum_next(recv_tloeframe->seq_num_ack));

	// Update sequence numbers
	// Note that next_rx_seq must not be increased
	e->acked_seq = recv_tloeframe->seq_num_ack; // we need the acked_seq?
	e->ack_cnt++;

	if (e->ack_cnt % 100 == 0) {
		fprintf(stderr, "next_tx: %d, ackd: %d, next_rx: %d, ack_cnt: %d\n",
				e->next_tx_seq, e->acked_seq, e->next_rx_seq, e->ack_cnt);
	}
}

static void serve_normal_request(tloe_endpoint_t *e, TloeFrame *recv_tloeframe) {
	// Handle TileLink Msg
	// TODO
	// printf("RX: Send pakcet to Tx channel for replying ACK/NAK with seq_num: %d, seq_num_ack: %d, ack: %d\n",
	//    tloeframe->seq_num, tloeframe->seq_num_ack, tloeframe->ack);

	// do something for handling the TileLink Msg

	// Delayed ACK
	if (e->timeout_rx.ack_pending == 0) {
		e->timeout_rx.ack_pending = 1;
		e->timeout_rx.ack_time = get_current_timestamp(&(e->iteration_ts));
	}
	e->timeout_rx.last_ack_seq = recv_tloeframe->seq_num;
	// Update sequence numbers
	e->next_rx_seq = tloe_seqnum_next(recv_tloeframe->seq_num);
	e->acked_seq = recv_tloeframe->seq_num_ack;
	e->timeout_rx.last_channel = recv_tloeframe->tlmsg.channel;
	e->timeout_rx.last_credit = get_tlmsg_credit(&(recv_tloeframe->tlmsg));		//TODO

	e->timeout_rx.ack_cnt++;
	e->delay_cnt++;
}

static void serve_duplicate_request(tloe_endpoint_t *e, TloeFrame *recv_tloeframe) {
	int seq_num = recv_tloeframe->seq_num;
	fprintf(stderr, "TLoE frame is a duplicate. "
			"seq_num: %d, next_rx_seq: %d\n",
			seq_num, e->next_rx_seq);

	// If the received frame contains data, enqueue it in the message buffer
	BUG_ON(is_ack_msg(recv_tloeframe), "received frame must not be an ack frame.");

	// Delayed ACK
	if (e->timeout_rx.ack_pending == 0) {
		e->timeout_rx.ack_pending = 1;
		e->timeout_rx.ack_time = get_current_timestamp(&(e->iteration_ts));
		e->timeout_rx.last_ack_seq = recv_tloeframe->seq_num;
	} else if(tloe_seqnum_cmp(recv_tloeframe->seq_num, e->timeout_rx.last_ack_seq) > 0) {
		e->timeout_rx.last_ack_seq = recv_tloeframe->seq_num;
	} 

	e->timeout_rx.ack_cnt++;
	e->delay_cnt++;
	e->dup_cnt++;
}

static void serve_oos_request(tloe_endpoint_t *e, TloeFrame *recv_tloeframe) {
	int enqueued;
	// The received TLoE frame is out of sequence, indicating that some frames were lost
	// The frame should be dropped, NEXT_RX_SEQ is not updated
	// A negative acknowledgment (NACK) is sent using the last properly received sequence number
	int last_proper_rx_seq = tloe_seqnum_prev(e->next_rx_seq);

	fprintf(stderr, "TLoE frame is out of sequence with "
			"seq_num: %d, next_rx_seq: %d last: %d\n",
			recv_tloeframe->seq_num, e->next_rx_seq, last_proper_rx_seq);

	// If the received frame contains data, enqueue it in the message buffer
	BUG_ON(is_ack_msg(recv_tloeframe), "received frame must not be an ack frame.");

	recv_tloeframe->seq_num_ack = last_proper_rx_seq;
	recv_tloeframe->ack = TLOE_NAK;  // NAK
	recv_tloeframe->mask = 0; // To indicate ACK
	enqueued = enqueue(e->ack_buffer, (void *) recv_tloeframe);
	BUG_ON(!enqueued, "failed to enqueue ack frame.");

	init_timeout_rx(&(e->iteration_ts), &(e->timeout_rx));

	e->oos_cnt++;
}

static void enqueue_ack_frame(tloe_endpoint_t *e, TloeFrame *recv_tloeframe) {
	int enqueued;
	TloeFrame *frame = malloc(sizeof(TloeFrame));
	BUG_ON(!frame, "failed to allocate memory for ack frame.");

	*frame = *recv_tloeframe;
	frame->seq_num = e->timeout_rx.last_ack_seq;
	frame->seq_num_ack = e->timeout_rx.last_ack_seq;
	frame->ack = TLOE_ACK;
	frame->mask = 0;
	frame->channel = e->timeout_rx.last_channel;
	frame->credit = e->timeout_rx.last_credit;
	enqueued = enqueue(e->ack_buffer, (void *) frame);
	BUG_ON(!enqueued, "failed to enqueue ack frame.");

	e->timeout_rx.ack_pending = 0;
	e->timeout_rx.ack_cnt = 0;
	e->delay_cnt--;
}

void RX(tloe_endpoint_t *e) {
	int size;
	chan_credit_t chan_credit;
	tloe_rx_req_type_t req_type;
	TloeFrame *recv_tloeframe = (TloeFrame *)malloc(sizeof(TloeFrame));
	if (!recv_tloeframe) {
		fprintf(stderr, "%s[%d] failed to allocate memory for recv_tloeframe\n", __FILE__, __LINE__);
		goto out;
	}

	// Receive a frame from the Ethernet layer
	size = tloe_ether_recv(e->ether, (char *)recv_tloeframe);
	if (size == 0) {
		free(recv_tloeframe);
		goto process_ack;
	}

	// Connection/Disconnection 
	if (is_conn_msg(recv_tloeframe)) {
		serve_conn(e, recv_tloeframe);
	}	

	// ACK/NAK (zero-TileLink)
	if (is_ack_msg(recv_tloeframe)) {
		serve_ack(e, recv_tloeframe);
		free(recv_tloeframe);

		goto process_ack;
	}

#ifdef TEST_NORMAL_FRAME_DROP // (Test) Delayed ACK: Drop a certain number of normal packets
	if (e->master == 0) {  // in case of slave
		static int dack;
		if (dack == 0) {
			if ((rand() % 1000) < 1) {
				dack == 1;
				e->drop_npacket_size = 4;
			}
		}

		if (e->drop_npacket_size-- > 0) {
			//printf("Drop normal packet of seq_num %d\n", recv_tloeframe->seq_num);
			e->drop_npacket_cnt++;
			free(recv_tloeframe);
			if (e->drop_npacket_size == 0) dack = 0;
			goto process_ack;
		}
	}
#endif
	
	// printf("RX: Received packet with seq_num: %d, seq_num_ack: %d, ack: %d\n",
	//     tloeframe->seq_num, tloeframe->seq_num_ack, tloeframe->ack);
	req_type = tloe_rx_get_req_type(e, recv_tloeframe);
	switch (req_type) {
		case REQ_NORMAL:
#ifdef TEST_NORMAL_FRAME_DROP
			if ((rand() % 10000) == 99) {
				e->drop_cnt++;
				free(recv_tloeframe);
				goto process_ack;
			}
#endif
			// Normal request packet
			// Handle and enqueue it into the message buffer
			// Handle normal request packet
			serve_normal_request(e, recv_tloeframe);
			free(recv_tloeframe);
			// if (next_tx_seq % 100 == 0)
			// fprintf(stderr, "next_tx: %d, ackd: %d, next_rx: %d, ack_cnt: %d\n",
			//     next_tx_seq, acked_seq, next_rx_seq, ack_cnt);
			break;
		case REQ_DUPLICATE:
			serve_duplicate_request(e, recv_tloeframe);
			free(recv_tloeframe);
			break;
		case REQ_OOS:
			// recv_tloeframe is not freed here because of the enqueue
			serve_oos_request(e, recv_tloeframe);
			break;
	}

process_ack:
	// Send a delayed ACK if the timeout has occurred
	if (is_send_delayed_ack(&(e->iteration_ts), &(e->timeout_rx)))
		enqueue_ack_frame(e, recv_tloeframe);
out:
}

