#include "tloe_endpoint.h"
#include "tloe_connection.h"
#include "tloe_seq_mgr.h"
#include "flowcontrol.h"

static void send_frame(tloe_endpoint_t *e, int type, uint32_t seq_num, uint32_t seq_num_ack, int chan, int credit) {
    tloe_frame_t f;
    memset((void *)&f, 0, sizeof(tloe_frame_t));
    char send_buffer[MAX_BUFFER_SIZE];

    // Set Open Connection
    f.header.type = type;
    // Update the sequence number
    //tloe_seqnum_set_next_tx_seq(&f, e);
    f.header.seq_num = seq_num;
    // Update the sequence number of the ack
    //tloe_seqnum_set_frame_seq_num_ack(&f, e->acked_seq);
    f.header.seq_num_ack = seq_num_ack;
    // Set the ack to TLOE_ACK
    f.header.ack = TLOE_ACK;
    // Set the mask to indicate normal packet
    tloe_set_mask(&f, 0, CONN_PACKET_SIZE);
    // Set 0 to channel and credit
    f.header.chan = chan;
    f.header.credit = credit;
    // Convert tloe_frame into packet
    tloe_frame_to_packet((tloe_frame_t *)&f, send_buffer, CONN_PACKET_SIZE);
    // Send the request_normal_frame using the ether
    tloe_fabric_send(e, send_buffer, CONN_PACKET_SIZE);
}

////////
// Open Connection

static void recv_conn_master(tloe_endpoint_t *ep) {
    char recv_buffer[MAX_BUFFER_SIZE];
    int size;

    size = tloe_fabric_recv(ep, recv_buffer, sizeof(recv_buffer));
    if (size >= 0) {
        tloe_frame_t recv_tloeframe;

        // Convert packet into tloe_frame
        packet_to_tloe_frame(recv_buffer, size, &recv_tloeframe);

        // Set credits
        if(tloe_seqnum_cmp(recv_tloeframe.header.seq_num, ep->next_rx_seq) == 0) {
            set_credit(&(ep->fc), recv_tloeframe.header.chan, recv_tloeframe.header.credit);
            // Update sequence numbers
            tloe_seqnum_update_next_rx_seq(ep, &recv_tloeframe);
            tloe_seqnum_update_acked_seq(ep, &recv_tloeframe);
        }
    }
}

static int recv_conn_slave(tloe_endpoint_t *ep, int conn_flag) {
    char recv_buffer[MAX_BUFFER_SIZE];
    int size;

    size = tloe_fabric_recv(ep, recv_buffer, sizeof(recv_buffer));
    if (size >= 0) {
        tloe_frame_t recv_tloeframe;

        // Convert packet into tloe_frame
        packet_to_tloe_frame(recv_buffer, size, &recv_tloeframe);

        if (recv_tloeframe.header.type == TYPE_OPEN_CONNECTION) {
            conn_flag = 1;
        }

        if (conn_flag) {  
            set_credit(&(ep->fc), recv_tloeframe.header.chan, recv_tloeframe.header.credit);
            // Update sequence numbers
            tloe_seqnum_update_next_rx_seq(ep, &recv_tloeframe);
            tloe_seqnum_update_acked_seq(ep, &recv_tloeframe);
        }
    }

    return conn_flag;
}

static void serve_conn_master(tloe_endpoint_t *ep) {
    char send_buffer[MAX_BUFFER_SIZE];

    for (int i = 1; i < CHANNEL_NUM; i++) {
        tloe_frame_t tloeframe;
        memset(&tloeframe, 0, sizeof(tloeframe));

        send_frame(ep, (i == CHANNEL_A ? TYPE_OPEN_CONNECTION : TYPE_NORMAL), 
                i-1, tloe_seqnum_prev(ep->next_rx_seq), i, CREDIT_DEFAULT);

        ep->next_tx_seq = i;
    }
}

static void serve_conn_slave(tloe_endpoint_t *ep) {
    char send_buffer[MAX_BUFFER_SIZE];

    for (int i = 1; i < CHANNEL_NUM; i++) {
        tloe_frame_t tloeframe;
        memset(&tloeframe, 0, sizeof(tloeframe));

        send_frame(ep, TYPE_NORMAL, i-1, tloe_seqnum_prev(ep->next_rx_seq), i, CREDIT_DEFAULT);

        ep->next_tx_seq = i;
    }
}

static int check_timer(struct timespec *start) {
    struct timespec now;
    long elapsed_us;

    clock_gettime(CLOCK_MONOTONIC, &now);
    elapsed_us = (now.tv_sec - start->tv_sec) * 1000000L + (now.tv_nsec - start->tv_nsec) / 1000L;

    if (elapsed_us >= CONN_RESEND_TIME) 
        return 1;
    
    return 0;
}

int open_conn_master(tloe_endpoint_t *ep) {
    int is_done = 0;
    struct timespec start, now;
    int timer = 1;

    while (!is_done) {
        if (timer == 1) {
            serve_conn_master(ep);
            clock_gettime(CLOCK_MONOTONIC, &start);
            timer = 0;
        }

        // Receive packet and set credit
        recv_conn_master(ep);

        is_done = check_all_channels(&(ep->fc));

        timer = check_timer(&start);
    }

    send_frame(ep, TYPE_NORMAL, ep->next_tx_seq, tloe_seqnum_prev(ep->next_rx_seq), 0, 0);
    tloe_seqnum_next_tx_seq_inc(ep);

    return is_done;
}
 
int open_conn_slave(tloe_endpoint_t *ep) {
    int is_done = 0;
    int conn_flag = 0;
    struct timespec start, now;
    int timer = 1;

    printf("Wait until all channels have their credits set.....\n");

    while (!is_done) {
        if (conn_flag && timer == 1) {
            serve_conn_slave(ep);
            clock_gettime(CLOCK_MONOTONIC, &start);
            timer = 0;
        }

        conn_flag = recv_conn_slave(ep, conn_flag);

        is_done = check_all_channels(&(ep->fc));

        timer = check_timer(&start);
    }

    send_frame(ep, TYPE_ACKONLY, ep->next_tx_seq, tloe_seqnum_prev(ep->next_rx_seq), 0, 0);

    return is_done;
}

////////
// Close Connection

static void serve_close_conn(tloe_endpoint_t *ep) {
    char send_buffer[MAX_BUFFER_SIZE];

    tloe_frame_t tloeframe;
    memset(&tloeframe, 0, sizeof(tloeframe));

    send_frame(ep, TYPE_CLOSE_CONNECTION, ep->next_tx_seq, tloe_seqnum_prev(ep->next_rx_seq), 0, 0);
}

static int recv_close_conn_master(tloe_endpoint_t *ep) {
    char recv_buffer[MAX_BUFFER_SIZE];
    int size;
    int result = 0;

    size = tloe_fabric_recv(ep, recv_buffer, sizeof(recv_buffer));
    if (size >= 0) {
        tloe_frame_t recv_tloeframe;

        // Convert packet into tloe_frame
        packet_to_tloe_frame(recv_buffer, size, &recv_tloeframe);

        if (recv_tloeframe.header.type == TYPE_CLOSE_CONNECTION) {

            result = 1;

            // Update sequence numbers
            ep->next_rx_seq = tloe_seqnum_next(recv_tloeframe.header.seq_num);
            ep->acked_seq = recv_tloeframe.header.seq_num_ack;
        }
    }

    return result;
}

static int recv_close_conn_slave(tloe_endpoint_t *ep, int conn_flag) {
    char recv_buffer[MAX_BUFFER_SIZE];
    int size;
    int result = 0;

    size = tloe_fabric_recv(ep, recv_buffer, sizeof(recv_buffer));
    if (size >= 0) {
        tloe_frame_t recv_tloeframe;

        // Convert packet into tloe_frame
        packet_to_tloe_frame(recv_buffer, size, &recv_tloeframe);

        if (recv_tloeframe.header.type == TYPE_CLOSE_CONNECTION) {

            conn_flag = 1;

            if (conn_flag) { 
                // Update sequence numbers
                ep->next_rx_seq = tloe_seqnum_next(recv_tloeframe.header.seq_num);
                ep->acked_seq = recv_tloeframe.header.seq_num_ack;
            }
        }
    }

    return conn_flag;
}

int close_conn_master(tloe_endpoint_t *ep) {
    int is_done = 0;
    struct timespec start, now;
    int timer = 1;

    while (!is_done) {
        if (timer == 1) {
            serve_close_conn(ep);
            clock_gettime(CLOCK_MONOTONIC, &start);
            timer = 0;
        }

        is_done = recv_close_conn_master(ep);

        timer = check_timer(&start);
    }

    return is_done;
}


int close_conn_slave(tloe_endpoint_t *ep) {
    int is_done = 0;
    int conn_flag = 0;

    while (!is_done) {
        if (conn_flag) {
            serve_close_conn(ep);
            is_done = 1;
        }

        conn_flag = recv_close_conn_slave(ep, conn_flag);
    }

    return is_done;
}

int is_conn(tloe_endpoint_t *e) {
    int result = 0;
    if (e->connection == 0)
        printf("Initiate the connection first.\n");
    else
        result = 1;

    return result;
}
