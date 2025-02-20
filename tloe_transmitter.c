#include <stdio.h>
#include <stddef.h>

#include "tloe_common.h"
#include "tloe_transmitter.h"
#include "tloe_endpoint.h"
#include "tloe_frame.h"
#include "retransmission.h"
#include "timeout.h"

void open_conn(tloe_endpoint_t *e) {
    TloeEther *ether = e->ether;
    char send_buffer[MAX_BUFFER_SIZE];

    for (int i = 1; i < CHANNEL_NUM; i++) {
        tloe_frame_t tloeframe;
        memset(&tloeframe, 0, sizeof(tloeframe));

        tloeframe.header.seq_num = e->next_tx_seq;
        tloeframe.header.seq_num_ack = e->acked_seq;
        tloeframe.header.type = 2;
        tloeframe.header.chan = i;
        tloeframe.header.credit = CREDIT_DEFAULT;
        tloe_set_mask(&tloeframe, 0);
        // Convert tloe_frame into packet
        tloe_frame_to_packet(&tloeframe, send_buffer, sizeof(tloe_frame_t));
        //tloe_ether_send(ether, (char *)&tloeframe, sizeof(tloe_frame_t));
        tloe_ether_send(ether, send_buffer, sizeof(tloe_frame_t));

        e->next_tx_seq = tloe_seqnum_next(e->next_tx_seq);
    }
}

static int enqueue_retransmit_buffer(tloe_endpoint_t *e, RetransmitBufferElement *rbe, tl_msg_t *tlmsg) {
	// Update the sequence number
	rbe->tloe_frame.header.seq_num = e->next_tx_seq;
	// Set the tilelink msg
	tloe_set_tlmsg(&(rbe->tloe_frame), tlmsg, 0);
	// Set the state to TLOE_INIT
	rbe->state = TLOE_INIT;
	// Get the current time
	rbe->send_time = get_current_timestamp(&(e->iteration_ts));
	return enqueue(e->retransmit_buffer, rbe);
}

static void send_request_normal_tlmsg(tloe_endpoint_t *e, tl_msg_t *tlmsg) {
    tloe_frame_t *f = (tloe_frame_t *)malloc(sizeof(tloe_frame_t));
    char send_buffer[MAX_BUFFER_SIZE];

    // Update the sequence number
    f->header.seq_num = e->next_tx_seq;
    // Update the sequence number of the ack
    f->header.seq_num_ack = e->acked_seq;
    // Set the ack to TLOE_ACK
    f->header.ack = TLOE_ACK;
    // Set the mask to indicate normal packet
    tloe_set_mask(f, 1);
    // Set the tilelink msg
    tloe_set_tlmsg(f, tlmsg, 0);
    // Set 0 to channel and credit
    f->header.chan = 0;
    f->header.credit = 0;

    // Convert tloe_frame into packet
    tloe_frame_to_packet((tloe_frame_t *)f, send_buffer, sizeof(tloe_frame_t));
    // Send the request_normal_frame using the ether
    tloe_ether_send(e->ether, send_buffer, sizeof(tloe_frame_t));

    // Free tloe_frame_t
    free(f);
}

tl_msg_t *TX(tloe_endpoint_t *e, tl_msg_t *request_normal_tlmsg) {
    int enqueued;
    tl_msg_t *return_tlmsg = NULL;
    RetransmitBufferElement *rbe;
    char send_buffer[MAX_BUFFER_SIZE];

    if (!is_queue_empty(e->ack_buffer)) {
        tloe_frame_t *ack_frame;

        //		return_tlmsg = request_normal_tlmsg;
        ack_frame = (tloe_frame_t *)dequeue(e->ack_buffer);

#ifdef TEST_TIMEOUT_DROP // (Test) Delayed ACK: Drop a certain number of ACK packets
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
        ack_frame->header.seq_num = e->next_tx_seq;

        // Convert tloe_frame into packet
        tloe_frame_to_packet(ack_frame, send_buffer, sizeof(tloe_frame_t)); 
        tloe_ether_send(e->ether, send_buffer, sizeof(tloe_frame_t));

        // ack_frame must be freed because of the dequeue
        free(ack_frame);
	} 
	if (request_normal_tlmsg && !is_queue_full(e->retransmit_buffer)) {
		// NORMAL packet
		// Reflect the sequence number, store in the retransmission buffer, and send
		// Check credt for flow control
		if (dec_credit(&(e->fc), request_normal_tlmsg->header.chan, 1) == 0) {
			return_tlmsg = request_normal_tlmsg;
			goto out;
		} else {
			e->fc_dec_cnt++;
		}
		
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
	if ((rbe = getfront(e->retransmit_buffer)) && is_timeout_tx(&(e->iteration_ts), rbe->send_time)) {
		fprintf(stderr, "TX: Timeout TX and retranmission from seq_num: %d\n", rbe->tloe_frame.header.seq_num);
		retransmit(e, rbe->tloe_frame.header.seq_num);
	}
out:
	return return_tlmsg;
}
