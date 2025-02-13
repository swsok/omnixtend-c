#include <stdio.h>
#include <stddef.h>

#include "tloe_common.h"
#include "tloe_transmitter.h"
#include "tloe_endpoint.h"
#include "retransmission.h"
#include "timeout.h"

static int enqueue_retransmit_buffer(tloe_endpoint_t *e, RetransmitBufferElement *rbe, TileLinkMsg *tlmsg) {
	// Update the sequence number
	rbe->tloe_frame.seq_num = e->next_tx_seq;
	// Set the tilelink msg
	rbe->tloe_frame.tlmsg = *tlmsg;
	// Set the state to TLOE_INIT
	rbe->state = TLOE_INIT;
	// Get the current time
	rbe->send_time = get_current_time();
	return enqueue(e->retransmit_buffer, rbe);
}

static void send_request_normal_tlmsg(tloe_endpoint_t *e, TileLinkMsg *tlmsg) {
	TloeFrame *f = (TloeFrame *)malloc(sizeof(TloeFrame));

	// Update the sequence number
	f->seq_num = e->next_tx_seq;
	// Update the sequence number of the ack
	f->seq_num_ack = e->acked_seq;
	// Set the ack to TLOE_ACK
	f->ack = TLOE_ACK;
	// Set the mask to indicate normal packet
	f->mask = 1;
	// Set the tilelink msg
	f->tlmsg = *tlmsg;
	// Send the request_normal_frame using the ether
	tloe_ether_send(e->ether, (char *)f, sizeof(TloeFrame));
	// Free TloeFrame
	free(f);
}

TileLinkMsg *TX(tloe_endpoint_t *e, TileLinkMsg *request_normal_tlmsg) {
	int enqueued;
	TileLinkMsg *return_tlmsg = NULL;
	RetransmitBufferElement *rbe;

	if (!is_queue_empty(e->ack_buffer)) {
		TloeFrame *ack_frame;

		return_tlmsg = request_normal_tlmsg;
		ack_frame = (TloeFrame *)dequeue(e->ack_buffer);

#if 1 // (Test) Delayed ACK: Drop a certain number of ACK packets
		if (e->master == 0) {  // in case of slave
			static int dack;
			if (dack == 0) {
				if ((rand() % 1000) < 1) {
					dack == 1;
					e->drop_apacket_size = 4;
				}
			}

			if (e->drop_apacket_size-- > 0) {
				//printf("Drop ack packet of seq_num %d\n", recv_tloeframe->seq_num);
				e->drop_apacket_cnt++;
				if (e->drop_npacket_size == 0) dack = 0;
				free(ack_frame);
				goto out;
			}
		}
#endif
		// ACK/NAK packet (zero-TileLink)
		// Reflect the sequence number but do not store in the retransmission buffer, just send
		ack_frame->seq_num = e->next_tx_seq;
		tloe_ether_send(e->ether, (char *)ack_frame, sizeof(TloeFrame));

		// ack_frame must be freed because of the dequeue
		free(ack_frame);
	} else if (request_normal_tlmsg && !is_queue_full(e->retransmit_buffer)) {
		// NORMAL packet
		// Reflect the sequence number, store in the retransmission buffer, and send
		// Enqueue to retransmitBuffer
		rbe = (RetransmitBufferElement *)malloc(sizeof(RetransmitBufferElement));
		if (!rbe) {
			fprintf(stderr, "%s[%d] failed to allocate memory for retransmit buffer element.\n"
							"the requested frame must be retransmitted for the next TX",
					__FILE__, __LINE__);
			return_tlmsg = request_normal_tlmsg;
			goto out;
		}

		// Enqueue to retransmit buffer
		enqueued = enqueue_retransmit_buffer(e, rbe, request_normal_tlmsg);
		BUG_ON(!enqueued, "failed to enqueue retransmit buffer element.");

		// Send the request_normal_tlmsg
		send_request_normal_tlmsg(e, request_normal_tlmsg);
		// Set the state to TLOE_SENT
		rbe->state = TLOE_SENT;

		// Increase the sequence number of the endpoint
		e->next_tx_seq = tloe_seqnum_next(e->next_tx_seq);
	} else {
		// If the retransmit buffer is full or the request_normal_tlmsg is NULL, 
		// return the request_normal_tlmsg for the next TX
		return_tlmsg = request_normal_tlmsg;
	}

	// Retransmit all the elements in the retransmit buffer if the timeout has occurred
	if ((rbe = getfront(e->retransmit_buffer)) && is_timeout_tx(rbe->send_time)) {
		fprintf(stderr, "TX: Timeout TX and retranmission from seq_num: %d\n", rbe->tloe_frame.seq_num);
		retransmit(e, rbe->tloe_frame.seq_num);
	}
out:
	return return_tlmsg;
}
