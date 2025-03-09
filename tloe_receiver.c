#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "tloe_receiver.h"
#include "tloe_frame.h"
#include "tloe_endpoint.h"
#include "tloe_common.h"
#include "tloe_connection.h"
#include "retransmission.h"
#include "timeout.h"

static void serve_ack(tloe_endpoint_t *e, tloe_frame_t *recv_tloeframe) {
    // Slide retransmission buffer for flushing ancester frames
    // Note that ACK/NAK transmit the sequence number of the received frame as seq_num_ack
    slide_window(e, recv_tloeframe);

    // Additionally, NAK re-transmit the frames in the retransmission buffer
    if (recv_tloeframe->header.ack == TLOE_NAK)  // NAK
        retransmit(e, tloe_seqnum_next(recv_tloeframe->header.seq_num_ack));

    e->ack_cnt++;

    if (e->ack_cnt % 100 == 0) {
        fprintf(stderr, "next_tx: %d, ackd: %d, next_rx: %d, ack_cnt: %d\n",
                e->next_tx_seq, e->acked_seq, e->next_rx_seq, e->ack_cnt);
    }
}

static int add_channel_flow_credits(tloe_endpoint_t *e, int channel, int total_flits) {
    e->fc.tx_flow_credits[channel] += total_flits;
}

static void update_next_rx_seq(tloe_endpoint_t *e, tloe_frame_t *frame) {
    e->next_rx_seq = tloe_seqnum_next(frame->header.seq_num);
}

static void update_acked_seq(tloe_endpoint_t *e, tloe_frame_t *frame) {
    if (tloe_seqnum_cmp(frame->header.seq_num_ack, e->acked_seq) > 0) {
        e->acked_seq = frame->header.seq_num_ack;
    }
}

static void update_flow_control_credits(tloe_endpoint_t *e, tloe_frame_t *frame) {
    int prev_credit = e->fc.credits[frame->header.chan];
    int inc_credit = fc_credit_inc(&(e->fc), frame);

    if (inc_credit != -1) {
#if DEBUG
        DEBUG_PRINT("INCREASE credit : chan: %d,  %d  ->  %d (%d)\n", 
                frame->header.chan, prev_credit, 
                e->fc.credits[frame->header.chan], e->fc_inc_cnt);
#endif
        e->fc_inc_cnt++;
        e->fc_inc_value += inc_credit;
    }
}

static int serve_normal_request(tloe_endpoint_t *e, tloe_frame_t *recv_tloeframe, int f_size) {
    // Handle TileLink Msg
    int i, total_flits = 0;
    uint64_t mask;
    tl_msg_t *tlmsg;

    e->timeout_rx.ack_time = get_current_timestamp(&(e->iteration_ts));
    e->timeout_rx.last_ack_seq = recv_tloeframe->header.seq_num;

    // Update sequence numbers
    update_next_rx_seq(e, recv_tloeframe);
    update_acked_seq(e, recv_tloeframe);

    // Find tlmsgs from mask
    mask = tloe_get_mask(recv_tloeframe, f_size);

    i = 0;
    while (mask != 0) {
        if (mask & 1) {
            // Extract tlmsg from tloeframe located at i
            tlmsg = tloe_get_tlmsg(recv_tloeframe, i);

            // Extract tlmsg from the tloeframe located at index  
            if (!enqueue(e->tl_msg_buffer, (void *) tlmsg)) { 
                fprintf(stderr, "tl_msg_buffer overflow.\n");
                exit(1);
            }

            // Calculate flits for sending ack for flow-control
            total_flits += tlmsg_get_flits_cnt(tlmsg);
        }

        mask >>= 1;
        i++;
    }

    // Send ack for flow-control
    add_channel_flow_credits(e, tlmsg->header.chan, total_flits);

    // Increase credit
    update_flow_control_credits(e, recv_tloeframe);
}

static int serve_zero_tlmsg_request(tloe_endpoint_t *e, tloe_frame_t *recv_tloeframe) {
    // Update sequence numbers
    update_next_rx_seq(e, recv_tloeframe);
    update_acked_seq(e, recv_tloeframe);
    // Set for sending ackonly
    e->should_send_ackonly_frame = true;
    // Increase credit
    update_flow_control_credits(e, recv_tloeframe);
}

static void serve_duplicate_request(tloe_endpoint_t *e, tloe_frame_t *recv_tloeframe, int size) {
    int enqueued;
    int seq_num = recv_tloeframe->header.seq_num;
    tloe_frame_t *tloeframe = (tloe_frame_t *)malloc(sizeof(tloe_frame_t));
    memset((void *)tloeframe, 0, sizeof(tloe_frame_t));
    fprintf(stderr, "TLoE frame is a duplicate. "
            "seq_num: %d, next_rx_seq: %d\n",
            seq_num, e->next_rx_seq);

    // If the received frame contains data, enqueue it in the message buffer
    BUG_ON(is_zero_tl_frame(recv_tloeframe, size), "received frame must not be an ack frame.");

    *tloeframe = *recv_tloeframe;
    tloeframe->header.seq_num_ack = recv_tloeframe->header.seq_num;
    tloeframe->header.ack = TLOE_ACK;  // ACK
    tloeframe->header.chan = 0;
    tloeframe->header.credit = 0; 
    tloe_set_mask(tloeframe, 0, size);
    enqueued = enqueue(e->ack_buffer, (void *) tloeframe);
    BUG_ON(!enqueued, "failed to enqueue ack frame.");

    e->dup_cnt++;
}

static void serve_oos_request(tloe_endpoint_t *e, tloe_frame_t *recv_tloeframe, int size, uint32_t seq_num) {
    int enqueued;
    // The received TLoE frame is out of sequence, indicating that some frames were lost
    // The frame should be dropped, NEXT_RX_SEQ is not updated
    // A negative acknowledgment (NACK) is sent using the last properly received sequence number
    uint32_t last_proper_rx_seq = seq_num;
    tloe_frame_t *tloeframe = (tloe_frame_t *)malloc(sizeof(tloe_frame_t));
    memset((void *)tloeframe, 0, sizeof(tloe_frame_t));
    fprintf(stderr, "TLoE frame is out of sequence with "
            "seq_num: %d, next_rx_seq: %d, last: %d\n",
            recv_tloeframe->header.seq_num, e->next_rx_seq, last_proper_rx_seq);

    // If the received frame contains data, enqueue it in the message buffer
    BUG_ON(is_zero_tl_frame(recv_tloeframe, size), "received frame must not be an ack frame.");

    *tloeframe = *recv_tloeframe;
    tloeframe->header.seq_num_ack = last_proper_rx_seq;
    tloeframe->header.ack = TLOE_NAK;  // NAK
    tloe_set_mask(tloeframe, 0, size);
    tloeframe->header.chan = 0;
    tloeframe->header.credit = 0; 
    enqueued = enqueue(e->ack_buffer, (void *) tloeframe);
    BUG_ON(!enqueued, "failed to enqueue ack frame.");

    init_timeout_rx(&(e->iteration_ts), &(e->timeout_rx));

    e->oos_cnt++;
}

void RX(tloe_endpoint_t *e) {
    int size;
    chan_credit_t chan_credit;
    int inc_credit;
    tloe_rx_req_type_t req_type;
    char recv_buffer[MAX_BUFFER_SIZE];
    tloe_frame_t *recv_tloeframe = (tloe_frame_t *)malloc(sizeof(tloe_frame_t));
    if (!recv_tloeframe) {
        fprintf(stderr, "%s[%d] failed to allocate memory for recv_tloeframe\n", __FILE__, __LINE__);
        goto out;
    }
    memset((void *)recv_tloeframe, 0, sizeof(tloe_frame_t));

    // Receive a frame from the Ethernet layer
    size = tloe_fabric_recv(e, recv_buffer, sizeof(recv_buffer));
    if (size < 0) {
        free(recv_tloeframe);
        goto out;
    }

    // Convert packet into tloe_frame
    packet_to_tloe_frame(recv_buffer, size, recv_tloeframe);

    serve_ack(e, recv_tloeframe);
    e->ackonly_frame_sent = false;

    // ACK/NAK (ACKONLY frame, spec 1.1)
    if (is_ackonly_frame(recv_tloeframe)) {
        if (tloe_seqnum_cmp(recv_tloeframe->header.seq_num_ack, e->acked_seq) > 0)
            e->acked_seq = recv_tloeframe->header.seq_num_ack;
        free(recv_tloeframe);
        goto out;
    }


#ifdef TEST_TIMEOUT_DROP // (Test) Delayed ACK: Drop a certain number of normal packets
    if (e->master == 0) {
        if (e->drop_npacket_size == 0 && ((rand() % 1000) == 0))
            e->drop_npacket_size = 4;
        if (e->drop_npacket_size > 0) {
            e->drop_npacket_cnt++;
            e->drop_npacket_size--;
            free(recv_tloeframe);
            goto out;
        }
    }
#endif

    req_type = tloe_rx_get_req_type(e, recv_tloeframe);
    switch (req_type) {
        case REQ_NORMAL:
            int inc_credit;

#ifdef TEST_NORMAL_FRAME_DROP
            if ((rand() % 10000) == 99) {
                e->drop_cnt++;
                free(recv_tloeframe);
                goto out;
            }
#endif
            // Normal packet
            if (!is_zero_tl_frame(recv_tloeframe, size)) {
                // Handle and enqueue it into the message buffer
                // Handle normal request packet
                serve_normal_request(e, recv_tloeframe, size);
            } else {
            // Zero tilelink frame
                serve_zero_tlmsg_request(e, recv_tloeframe);
            }
            free(recv_tloeframe);
            break;
        case REQ_DUPLICATE:
            serve_duplicate_request(e, recv_tloeframe, size);
            free(recv_tloeframe);
            break;
        case REQ_OOS:
            // recv_tloeframe is not freed here because of the enqueue
            serve_oos_request(e, recv_tloeframe, size, tloe_seqnum_prev(e->next_rx_seq));
            free(recv_tloeframe);
            break;
    }
out:
}

