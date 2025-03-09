#include <stdio.h>
#include <stddef.h>

#include "tloe_common.h"
#include "tloe_transmitter.h"
#include "tloe_endpoint.h"
#include "tloe_frame.h"
#include "retransmission.h"
#include "timeout.h"

static void send_ackonly_frame(tloe_endpoint_t *e) {
	char send_buffer[MAX_BUFFER_SIZE];
    tloe_frame_t f;
    int fsize = tloe_get_fsize(NULL);

    f.header.type = TYPE_ACKONLY;
    // Update the sequence number
    f.header.seq_num = e->next_tx_seq;
    // Update the sequence number of the ack
    f.header.seq_num_ack = tloe_seqnum_prev(e->next_rx_seq);
    // Set the ack to TLOE_ACK
    f.header.ack = TLOE_ACK;
    tloe_set_mask(&f, 0, fsize);
    // Set 0 to channel and credit
    f.header.chan = 0;
    f.header.credit = 0;
    // Convert tloe_frame into packet
    tloe_frame_to_packet((tloe_frame_t *)&f, send_buffer, fsize);
    // Send the request_normal_frame using the ether
    tloe_fabric_send(e, send_buffer, fsize);
}

static int enqueue_retransmit_buffer(tloe_endpoint_t *e, RetransmitBufferElement *rbe, tloe_frame_t *f, int f_size) {
    // Set the tloe frame
    memcpy(&rbe->tloe_frame, f, f_size);
    // Set the state to TLOE_INIT
    rbe->state = TLOE_INIT;
    rbe->f_size = f_size;
    // Get the current time
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
        // Set the tilelink msg
        tloe_set_tlmsg(f, tlmsg);
        // Set the mask to indicate normal packet
        tloe_set_mask(f, 1, f_size);
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
    tl_msg_t *return_tlmsg = NULL;
    RetransmitBufferElement *rbe;
    char send_buffer[MAX_BUFFER_SIZE];
    int tloeframe_size = 0;
    int enqueued;
    int target_credit_chan = select_max_credit_channel(&(e->fc));

    if (e->should_send_ackonly_frame) {
        send_ackonly_frame(e);
        e->should_send_ackonly_frame = false;
    }

    if (is_queue_full(e->retransmit_buffer)) {
        return_tlmsg = request_normal_tlmsg;
        goto out;
    }

    if (request_normal_tlmsg == NULL && is_queue_empty(e->ack_buffer) && target_credit_chan == 0)
        goto out;

    // Decrease credit based on the tilelink message
    if (request_normal_tlmsg) {
        int credit;

#if DEBUG
        DEBUG_PRINT("credit [A][%d] [D][%d]\n", e->fc.credits[CHANNEL_A], e->fc.credits[CHANNEL_D]);
#endif
        // Send tlmsg if credit is available 
        if ((credit = fc_credit_dec(&(e->fc), request_normal_tlmsg)) != -1) {
            e->fc_dec_cnt++;
            e->fc_dec_value += credit; 
        // If credit is not available but ack_buffer contains data, send credit using a zero-tlmsg
        } else if (!is_queue_empty(e->ack_buffer)) {
            return_tlmsg = request_normal_tlmsg;
            request_normal_tlmsg = NULL;
        } else {
            return_tlmsg = request_normal_tlmsg;
            goto out;
        }
    }

    // Get packet_size
    tloeframe_size = tloe_get_fsize(request_normal_tlmsg);
    BUG_ON(tloeframe_size == -1, "Frame size error");

    // Create normal frame
    tloe_frame_t *f = (tloe_frame_t *)malloc(tloeframe_size);
    memset((void *)f, 0, tloeframe_size); 
    prepare_normal_frame(e, f, request_normal_tlmsg, tloeframe_size);

    if (!is_queue_empty(e->ack_buffer)) {
        tloe_frame_t *ack_frame;

        ack_frame = (tloe_frame_t *)dequeue(e->ack_buffer);
        f->header.seq_num_ack = ack_frame->header.seq_num_ack;

        free(ack_frame);
    }

    // Handling Credit
    // if tx_flow_credits[channel] 2^, Send credit frame
    // NOTE: target_credit_chan != 0 means there is at least one credit to be sent for the corresponding channel
    if (target_credit_chan != 0) {
        // Set credit info in frame
        f->header.chan = target_credit_chan;
        f->header.credit = get_outgoing_credits(&(e->fc), target_credit_chan);
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
    memset((void *)rbe, 0, sizeof(RetransmitBufferElement));

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
    if ((rbe = getfront(e->retransmit_buffer)) && is_timeout_tx(&(e->iteration_ts), rbe->send_time)) {
        fprintf(stderr, "TX: Timeout TX and retranmission from seq_num: %d\n", rbe->tloe_frame.header.seq_num);
        retransmit(e, rbe->tloe_frame.header.seq_num);
    }
    return return_tlmsg;
}
