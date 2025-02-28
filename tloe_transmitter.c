#include <stdio.h>
#include <stddef.h>

#include "tloe_common.h"
#include "tloe_transmitter.h"
#include "tloe_endpoint.h"
#include "tloe_frame.h"
#include "retransmission.h"
#include "timeout.h"

static int enqueue_retransmit_buffer(tloe_endpoint_t *e, RetransmitBufferElement *rbe, tloe_frame_t *f, int f_size) {
    // Set the tloe frame
    memcpy(&rbe->tloe_frame, f, f_size);
    // Set the state to TLOE_INIT
    rbe->state = TLOE_INIT;
    // Get the current time
    //rbe->send_time = is_zero_tl_frame(f) ? get_current_timestamp(&(e->iteration_ts)) : 0;
    rbe->send_time = get_current_timestamp(&(e->iteration_ts));

    return enqueue(e->retransmit_buffer, rbe);
}

static void prepare_normal_frame(tloe_endpoint_t *e, tloe_frame_t *f, tl_msg_t *tlmsg, int f_size) {
        
    // Update the sequence number
    f->header.seq_num = e->next_tx_seq;
    // Update the sequence number of the ack
    f->header.seq_num_ack = tloe_seqnum_prev(e->next_rx_seq);
    // Set the ack to TLOE_ACK
    f->header.ack = TLOE_ACK;
    if (tlmsg) {
        // Set the mask to indicate normal packet
        tloe_set_mask(f, 1, f_size);
        // Set the tilelink msg
        tloe_set_tlmsg(f, tlmsg, 0);
    } else {
        tloe_set_mask(f, 0, f_size);
    }
    // Set 0 to channel and credit
    f->header.chan = 0;
    f->header.credit = 0;
}

static void send_request_normal_frame(tloe_endpoint_t *e, tloe_frame_t *f, int f_size) {
    char send_buffer[MAX_BUFFER_SIZE];

    // Convert tloe_frame into packet
    tloe_frame_to_packet((tloe_frame_t *)f, send_buffer, f_size);
    // Send the request_normal_frame using the ether
    tloe_fabric_send(e, send_buffer, f_size);
}

tl_msg_t *TX(tloe_endpoint_t *e, tl_msg_t *request_normal_tlmsg) {
    int enqueued;
    tl_msg_t *return_tlmsg = NULL;
    RetransmitBufferElement *rbe;
    char send_buffer[MAX_BUFFER_SIZE];
    int tloeframe_size = 0;

	if (is_queue_full(e->retransmit_buffer)) {
		return_tlmsg = request_normal_tlmsg;
        goto out;
    }

    if (request_normal_tlmsg == NULL && is_queue_empty(e->ack_buffer)) {
        goto out;
    }

    if (request_normal_tlmsg) {
        // Handle credit
        if (dec_credit(&(e->fc), request_normal_tlmsg->header.chan, 1) == 0) {
            return_tlmsg = request_normal_tlmsg;
            goto out;
        } else {
            e->fc_dec_cnt++;
        }
    }

    // Get packet_size
    if (request_normal_tlmsg == NULL)
        tloeframe_size = DEFAULT_FRAME_SIZE;
    else 
        tloeframe_size = tloe_get_fsize(request_normal_tlmsg);

    // Create normal frame
    tloe_frame_t *f = (tloe_frame_t *)malloc(tloeframe_size);
    memset((void *)f, 0, tloeframe_size); 
    prepare_normal_frame(e, f, request_normal_tlmsg, tloeframe_size);

    if (!is_queue_empty(e->ack_buffer)) {
        tloe_frame_t *ack_frame;

        ack_frame = (tloe_frame_t *)dequeue(e->ack_buffer);
        f->header.seq_num_ack = ack_frame->header.seq_num_ack;
        f->header.chan = ack_frame->header.chan;
        f->header.credit = ack_frame->header.credit;

        free(ack_frame);
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
    enqueued = enqueue_retransmit_buffer(e, rbe, f, tloeframe_size);
    BUG_ON(!enqueued, "failed to enqueue retransmit buffer element.");

    // Send normal frame
    // Send the request_normal_tlmsg
    send_request_normal_frame(e, f, tloeframe_size);
    // Set the state to TLOE_SENT
    rbe->state = TLOE_SENT;

    // Increase the sequence number of the endpoint
    e->next_tx_seq = tloe_seqnum_next(e->next_tx_seq);

out:
	// Retransmit all the elements in the retransmit buffer if the timeout has occurred
//	if ((rbe = get_earliest_element(e->retransmit_buffer)) && is_timeout_tx(&(e->iteration_ts), rbe->send_time)) {
	if ((rbe = getfront(e->retransmit_buffer)) && is_timeout_tx(&(e->iteration_ts), rbe->send_time)) {
		fprintf(stderr, "TX: Timeout TX and retranmission from seq_num: %d\n", rbe->tloe_frame.header.seq_num);
		retransmit(e, rbe->tloe_frame.header.seq_num);
	}
	return return_tlmsg;
}
