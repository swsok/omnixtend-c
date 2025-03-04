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

// TODO to-be modified
static void send_ackonly_frame(tloe_endpoint_t *e, tloe_frame_t *recv_frame, int size) {
	char send_buffer[MAX_BUFFER_SIZE];
    tloe_frame_t f;

    f.header.type = TYPE_ACKONLY;
    // Update the sequence number
    f.header.seq_num = e->next_tx_seq;
    // Update the sequence number of the ack
    f.header.seq_num_ack = recv_frame->header.seq_num;
    // Set the ack to TLOE_ACK
    f.header.ack = TLOE_ACK;
    tloe_set_mask(&f, 0, size);
    // Set 0 to channel and credit
    f.header.chan = 0;
    f.header.credit = 0;

    // Convert tloe_frame into packet
    tloe_frame_to_packet((tloe_frame_t *)&f, send_buffer, size);
    // Send the request_normal_frame using the ether
    tloe_fabric_send(e, send_buffer, size);
}

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

static void send_ack_per_tl(tloe_endpoint_t *e, tl_msg_t *tlmsg, int f_size) {
    int tl_chan = tlmsg->header.chan;
    int tl_opcode = tlmsg->header.opcode;
    int tl_size = tlmsg->header.size;

    int header_size = get_tl_header_size(tlmsg);
    int data_size = get_tl_data_size(tlmsg);
    int available_credit = -1;
    int enqueued;
    tloe_frame_t *tloeframe_header;
    tloe_frame_t *tloeframe_data;

    if (header_size >= 0) {
        tloeframe_header = (tloe_frame_t *)malloc(sizeof(tloe_frame_t));
        memset((void *)tloeframe_header, 0, sizeof(tloe_frame_t));

        tloeframe_header->header.seq_num_ack = tloe_seqnum_prev(e->next_rx_seq);
        tloeframe_header->header.ack = TLOE_ACK;  // ACK
        tloe_set_mask(tloeframe_header, 0, f_size);
        tloeframe_header->header.chan = tlmsg->header.chan;
#if 0   //TODO
        tloeframe_header->header.credit = header_size -3; 
#else
        tloeframe_header->header.credit = header_size -2; 
#endif

        enqueued = enqueue(e->ack_buffer, (void *) tloeframe_header);
        BUG_ON(!enqueued, "failed to enqueue ack frame.");
    }

#if 0
    if (data_size >= 0) {
        tloeframe_data = (tloe_frame_t *)malloc(sizeof(tloe_frame_t));
        memset((void *)tloeframe_data, 0, sizeof(tloe_frame_t));

        tloeframe_data->header.seq_num_ack = tloe_seqnum_prev(e->next_rx_seq);
        tloeframe_data->header.ack = TLOE_ACK;  // ACK
        tloe_set_mask(tloeframe_data, 0, f_size);
        tloeframe_data->header.chan = tlmsg->header.chan;
        tloeframe_data->header.credit = (data_size <= 2) ? 0 : (data_size -3); 

        enqueued = enqueue(e->ack_buffer, (void *) tloeframe_data);
        if (!enqueued)
            printf("SSSSS\n");
//        BUG_ON(!enqueued, "failed to enqueue ack frame.");
    }
#endif
}

static int serve_normal_request(tloe_endpoint_t *e, tloe_frame_t *recv_tloeframe, int f_size) {
    // printf("RX: Send pakcet to Tx channel for replying ACK/NAK with seq_num: %d, seq_num_ack: %d, ack: %d\n",
    //    tloeframe->header.seq_num, tloeframe->header.seq_num_ack, tloeframe->header.ack);
    // Handle TileLink Msg
    int i;
    tl_msg_t *tlmsg;

    e->timeout_rx.ack_time = get_current_timestamp(&(e->iteration_ts));
    e->timeout_rx.last_ack_seq = recv_tloeframe->header.seq_num;

    // Update sequence numbers
    e->next_rx_seq = tloe_seqnum_next(recv_tloeframe->header.seq_num);
    e->acked_seq = recv_tloeframe->header.seq_num_ack;
    e->timeout_rx.last_channel = tlmsg->header.chan;
    e->timeout_rx.last_credit = get_tlmsg_credit(tlmsg);

    // Find tlmsgs from mask
    uint64_t mask = tloe_get_mask(recv_tloeframe, f_size);
    for (i = 0; i < 64; i++) {
        if (mask & (1ULL << i)) {
            // Extract tlmsg and enqueue into tl_msg_buffer
            tlmsg = tloe_get_tlmsg(recv_tloeframe, i);

            if (!enqueue(e->tl_msg_buffer, (void *) tlmsg)) { 
                fprintf(stderr, "tl_msg_buffer overflow.\n");
                exit(1);
            }

            send_ack_per_tl(e, tlmsg, f_size);
        }
    }

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
    // TODO check chan/credit for dulpicated frame
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
    // TODO check chan/credit for oos frame
    tloeframe->header.chan = 0;
    tloeframe->header.credit = 0; 
    enqueued = enqueue(e->ack_buffer, (void *) tloeframe);
    BUG_ON(!enqueued, "failed to enqueue ack frame.");

    init_timeout_rx(&(e->iteration_ts), &(e->timeout_rx));

    e->oos_cnt++;
}

static void enqueue_ack_frame(tloe_endpoint_t *e, tloe_frame_t *recv_tloeframe) {
    int enqueued;
    tloe_frame_t *frame = malloc(sizeof(tloe_frame_t));
    BUG_ON(!frame, "failed to allocate memory for ack frame.");

    *frame = *recv_tloeframe;
    frame->header.seq_num = e->timeout_rx.last_ack_seq;
    frame->header.seq_num_ack = e->timeout_rx.last_ack_seq;
    frame->header.ack = TLOE_ACK;
    tloe_set_mask(frame, 0, 0); //TODO
    frame->header.chan = e->timeout_rx.last_channel;
    frame->header.credit = e->timeout_rx.last_credit;
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
    char recv_buffer[MAX_BUFFER_SIZE];
    tloe_frame_t *recv_tloeframe = (tloe_frame_t *)malloc(sizeof(tloe_frame_t));
    if (!recv_tloeframe) {
        fprintf(stderr, "%s[%d] failed to allocate memory for recv_tloeframe\n", __FILE__, __LINE__);
        goto out;
    }

    // Receive a frame from the Ethernet layer
    size = tloe_fabric_recv(e, recv_buffer, sizeof(recv_buffer));
    if (size < 0) {
        free(recv_tloeframe);
        goto process_ack;
    }

    // Convert packet into tloe_frame
    packet_to_tloe_frame(recv_buffer, size, recv_tloeframe);

    serve_ack(e, recv_tloeframe);

    // ACK/NAK (ACKONLY frame, spec 1.1)
    if (is_ackonly_frame(recv_tloeframe)) {
        e->acked_seq = recv_tloeframe->header.seq_num_ack;
        free(recv_tloeframe);
        goto process_ack;
    }

    // Zero tilelink frame
    if (is_zero_tl_frame(recv_tloeframe, size)) {
        e->next_rx_seq = tloe_seqnum_next(recv_tloeframe->header.seq_num);
        e->acked_seq = recv_tloeframe->header.seq_num_ack;

        // TODO move TX or delayed ack??
        // Send ACKONLY msg
        fprintf(stderr, "Send zero-tl frame\n");
        send_ackonly_frame(e, recv_tloeframe, size);

        free(recv_tloeframe);
        goto process_ack;
    }

#ifdef TEST_TIMEOUT_DROP // (Test) Delayed ACK: Drop a certain number of normal packets
    if (e->master == 0) {
        if (e->drop_npacket_size == 0 && ((rand() % 1000) == 0))
            e->drop_npacket_size = 4;
        if (e->drop_npacket_size > 0) {
            e->drop_npacket_cnt++;
            e->drop_npacket_size--;
            free(recv_tloeframe);
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
            serve_normal_request(e, recv_tloeframe, size);
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

process_ack:
	// Send a delayed ACK if the timeout has occurred
	//if (is_send_delayed_ack(&(e->iteration_ts), &(e->timeout_rx)))
//		enqueue_ack_frame(e, recv_tloeframe);
out:
}

