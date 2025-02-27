#include "tloe_endpoint.h"
#include "tloe_connection.h"
#include "flowcontrol.h"

static void serve_open_conn(tloe_endpoint_t *e, tloe_frame_t *recv_tloeframe) {
    char send_buffer[MAX_BUFFER_SIZE];
    // Set credits
    set_credit(&(e->fc), recv_tloeframe->header.chan, recv_tloeframe->header.credit);

    // If slave, send chan/credit message
    if (e->master == 0) {
        tloe_frame_t *f = (tloe_frame_t *)malloc(sizeof(tloe_frame_t));

        // Set Open Connection
        f->header.type = TYPE_OPEN_CONNECTION;
        // Update the sequence number
        f->header.seq_num = e->next_tx_seq;
        // Update the sequence number of the ack
        f->header.seq_num_ack = e->acked_seq;
        // Set the ack to TLOE_ACK
        f->header.ack = TLOE_ACK;
        // Set the mask to indicate normal packet
        tloe_set_mask(f, 0);
        // Set 0 to channel and credit
        f->header.chan = recv_tloeframe->header.chan;
        f->header.credit = CREDIT_DEFAULT;
        // Convert tloe_frame into packet
        tloe_frame_to_packet((tloe_frame_t *)f, send_buffer, sizeof(tloe_frame_t));
        // Send the request_normal_frame using the ether
        tloe_fabric_send(e, send_buffer, sizeof(tloe_frame_t));

        // increase the sequence number of the endpoint
        e->next_tx_seq = tloe_seqnum_next(e->next_tx_seq);
        // Free tloe_frame_t 
        free(f);

        init_timeout_rx(&(e->iteration_ts), &(e->timeout_rx));
   }
    // Update sequence numbers
    e->next_rx_seq = tloe_seqnum_next(recv_tloeframe->header.seq_num);
    e->acked_seq = recv_tloeframe->header.seq_num_ack;
}

static void serve_close_conn(tloe_endpoint_t *e, tloe_frame_t *recv_tloeframe) {
    char send_buffer[MAX_BUFFER_SIZE];

    // If slave, send chan/credit message
    if (e->master == 0) {
        tloe_frame_t *f = (tloe_frame_t *)malloc(sizeof(tloe_frame_t));

        // Set Open Connection
        f->header.type = TYPE_CLOSE_CONNECTION;
        // Update the sequence number
        f->header.seq_num = e->next_tx_seq;
        // Update the sequence number of the ack
        f->header.seq_num_ack = e->acked_seq;
        // Set the ack to TLOE_ACK
        f->header.ack = TLOE_ACK;
        // Set the mask to indicate normal packet
        tloe_set_mask(f, 0);
        // Set 0 to channel and credit
        f->header.chan = 0;
        f->header.credit = 0;
        // Convert tloe_frame into packet
        tloe_frame_to_packet((tloe_frame_t *)f, send_buffer, sizeof(tloe_frame_t));
        // Send the request_normal_frame using the ether
        tloe_fabric_send(e, send_buffer, sizeof(tloe_frame_t));

        // increase the sequence number of the endpoint
        e->next_tx_seq = tloe_seqnum_next(e->next_tx_seq);
        // Free tloe_frame_t 
        free(f);

        init_timeout_rx(&(e->iteration_ts), &(e->timeout_rx));
   }
    // Update sequence numbers
    e->next_rx_seq = tloe_seqnum_next(recv_tloeframe->header.seq_num);
    e->acked_seq = recv_tloeframe->header.seq_num_ack;
}

void open_conn_master(tloe_endpoint_t *e) {
    int size;
    char send_buffer[MAX_BUFFER_SIZE];
    char recv_buffer[MAX_BUFFER_SIZE];
    tloe_frame_t recv_tloeframe;
    struct timespec ts, start, now;

    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &start);

        while (1) {
            size = tloe_fabric_recv(e, recv_buffer, sizeof(recv_buffer));
            if (size >= 0) {
                // Convert packet into tloe_frame
                packet_to_tloe_frame(recv_buffer, size, &recv_tloeframe);

                if (is_conn_msg(&recv_tloeframe) == TYPE_OPEN_CONNECTION) {
                    serve_open_conn(e, &recv_tloeframe);
                }
            }

            clock_gettime(CLOCK_MONOTONIC, &now);
            long elapsed_us = (now.tv_sec - start.tv_sec) * 1000000L + (now.tv_nsec - start.tv_nsec) / 1000L;
#if 0
            if (elapsed_us >= TIMEOUT_TIME) {
                break;
            }
#endif
            usleep(100);
        }

        for (int i = 1; i < CHANNEL_NUM; i++) {
            tloe_frame_t tloeframe;
            memset(&tloeframe, 0, sizeof(tloeframe));

            if (is_filled_credit(&(e->fc), i))
                continue;

            tloeframe.header.seq_num = e->next_tx_seq;
            tloeframe.header.seq_num_ack = e->acked_seq;
            tloeframe.header.type = TYPE_OPEN_CONNECTION;
            tloeframe.header.chan = i;
            tloeframe.header.credit = CREDIT_DEFAULT;
            tloe_set_mask(&tloeframe, 0);

            // Convert tloe_frame into packet
            tloe_frame_to_packet(&tloeframe, send_buffer, sizeof(tloe_frame_t));
            tloe_fabric_send(e, send_buffer, sizeof(tloe_frame_t));

            e->next_tx_seq = tloe_seqnum_next(e->next_tx_seq);
        }

        if (!check_all_channels(&(e->fc))) {
            break;
        }
    }
    e->connection = 1;
}
 
void open_conn_slave(tloe_endpoint_t *e) {
    int size;
    char recv_buffer[MAX_BUFFER_SIZE];
    tloe_frame_t recv_tloeframe;

    printf("Wait until all channels have their credits set.....\n");
    while (check_all_channels(&(e->fc))) {
        size = tloe_fabric_recv(e, recv_buffer, sizeof(recv_buffer));
        if (size < 0) {
            //usleep(1000);
            continue;
        }

        // Convert packet into tloe_frame
        packet_to_tloe_frame(recv_buffer, size, &recv_tloeframe);

        if (is_conn_msg(&recv_tloeframe)) {
            serve_open_conn(e, &recv_tloeframe);
        }
    }
    e->connection = 1;
}

void close_conn_master(tloe_endpoint_t *e) {
    int size;
    char send_buffer[MAX_BUFFER_SIZE];
    char recv_buffer[MAX_BUFFER_SIZE];
    tloe_frame_t recv_tloeframe;
    struct timespec ts, start, now;
    e->connection = 0;

    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &start);

        while (1) {
            size = tloe_fabric_recv(e, recv_buffer, sizeof(recv_buffer));
            if (size >= 0) {
                // Convert packet into tloe_frame
                packet_to_tloe_frame(recv_buffer, size, &recv_tloeframe);

                if (is_conn_msg(&recv_tloeframe) == TYPE_CLOSE_CONNECTION) {
                    serve_close_conn(e, &recv_tloeframe);
                    goto out;
                }
            }

            clock_gettime(CLOCK_MONOTONIC, &now);
            long elapsed_us = (now.tv_sec - start.tv_sec) * 1000000L + (now.tv_nsec - start.tv_nsec) / 1000L;

            if (elapsed_us >= TIMEOUT_TIME) {
                break;
            }
            usleep(100);
        }

        tloe_frame_t tloeframe;
        memset(&tloeframe, 0, sizeof(tloeframe));

        tloeframe.header.seq_num = e->next_tx_seq;
        tloeframe.header.seq_num_ack = e->acked_seq;
        tloeframe.header.type = TYPE_CLOSE_CONNECTION;
        tloeframe.header.chan = 0;
        tloeframe.header.credit = 0;
        tloe_set_mask(&tloeframe, 0);

        // Convert tloe_frame into packet
        tloe_frame_to_packet(&tloeframe, send_buffer, sizeof(tloe_frame_t));
        tloe_fabric_send(e, send_buffer, sizeof(tloe_frame_t));

        e->next_tx_seq = tloe_seqnum_next(e->next_tx_seq);
    }
out:
}

void close_conn_slave(tloe_endpoint_t *e) {
    int size;
    char recv_buffer[MAX_BUFFER_SIZE];
    tloe_frame_t recv_tloeframe;
    e->connection = 0;

    printf("Wait until the connection is closed.\n");
    while (1) {
        size = tloe_fabric_recv(e, recv_buffer, sizeof(recv_buffer));
        if (size < 0) {
            //usleep(1000);
            continue;
        }

        // Convert packet into tloe_frame
        packet_to_tloe_frame(recv_buffer, size, &recv_tloeframe);

        if (is_conn_msg(&recv_tloeframe) == TYPE_CLOSE_CONNECTION) {
            serve_close_conn(e, &recv_tloeframe);
            break;
        }
    }
}

int is_conn(tloe_endpoint_t *e) {
    int result = 0;
    if (e->connection == 0)
        printf("Initiate the connection first.\n");
    else
        result = 1;

    return result;
}
