#include <stdio.h>
#include <stddef.h>

#include "tloe_common.h"
#include "tloe_transmitter.h"
#include "tloe_endpoint.h"
#include "retransmission.h"
#include "timeout.h"

static int enqueue_retransmit_buffer(tloe_endpoint_t *e, RetransmitBufferElement *rbe, TloeFrame *f) {
	// Copy the request_normal_frame to rbe
	rbe->tloe_frame = *f;
	// Update the sequence number
	rbe->tloe_frame.seq_num = e->next_tx_seq;
	// Set the state to TLOE_INIT
	rbe->state = TLOE_INIT;
	// Get the current time
	rbe->send_time = get_current_time();
	return enqueue(e->retransmit_buffer, rbe);
}

static void send_request_normal_frame(tloe_endpoint_t *e, TloeFrame *f) {
	// Update the sequence number
	f->seq_num = e->next_tx_seq;
	// Update the sequence number of the ack
	f->seq_num_ack = e->acked_seq;
	// Set the ack to TLOE_ACK
	f->ack = TLOE_ACK;
	// Set the mask to indicate normal packet
	f->mask = 1;
	// Send the request_normal_frame using the ether
	tloe_ether_send(e->ether, (char *)f, sizeof(TloeFrame));
}

TloeFrame *TX(tloe_endpoint_t *e, TloeFrame *request_normal_frame) {
	int enqueued;
	TloeFrame *return_frame = NULL;
	RetransmitBufferElement *rbe;

	if (!is_queue_empty(e->ack_buffer)) {
		TloeFrame *ack_frame;

		return_frame = request_normal_frame;
		ack_frame = (TloeFrame *)dequeue(e->ack_buffer);

		// ACK/NAK (zero-TileLink)
		// Reflect the sequence number but do not store in the retransmission buffer, just send
		ack_frame->seq_num = e->next_tx_seq;
		tloe_ether_send(e->ether, (char *)ack_frame, sizeof(TloeFrame));

		// ack_frame must be freed because of the dequeue
		free(ack_frame);
	} else if (request_normal_frame && !is_queue_full(e->retransmit_buffer)) {
		// If the request frame is not normal, it must be a BUG
		BUG_ON(is_ack_msg(request_normal_frame), "requested frame must not be an ack frame.");

		// NORMAL packet
		// Reflect the sequence number, store in the retransmission buffer, and send
		// Enqueue to retransmitBuffer
		rbe = (RetransmitBufferElement *)malloc(sizeof(RetransmitBufferElement));
		if (!rbe) {
			fprintf(stderr, "%s[%d] failed to allocate memory for retransmit buffer element.\n"
							"the requested frame must be retransmitted for the next TX",
					__FILE__, __LINE__);
			return_frame = request_normal_frame;
			goto out;
		}

		// Enqueue to retransmit buffer
		enqueued = enqueue_retransmit_buffer(e, rbe, request_normal_frame);
		BUG_ON(!enqueued, "failed to enqueue retransmit buffer element.");

		// Send the request_normal_frame
		send_request_normal_frame(e, request_normal_frame);
		// Set the state to TLOE_SENT
		rbe->state = TLOE_SENT;

		// Increase the sequence number of the endpoint
		e->next_tx_seq = tloe_seqnum_next(e->next_tx_seq);
	} else {
		// If the retransmit buffer is full or the request_normal_frame is NULL, 
		// return the request_normal_frame for the next TX
		return_frame = request_normal_frame;
	}

	// Retransmit all the elements in the retransmit buffer if the timeout has occurred
	if ((rbe = getfront(e->retransmit_buffer)) && is_timeout_tx(rbe->send_time)) {
		fprintf(stderr, "TX: Timeout TX and retranmission from seq_num: %d\n", rbe->tloe_frame.seq_num);
		retransmit(e, rbe->tloe_frame.seq_num);
	}
out:
	return return_frame;
}