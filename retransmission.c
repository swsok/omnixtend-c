#include "retransmission.h"
#include "tloe_endpoint.h"
#include "tloe_common.h"
#include "tloe_seq_mgr.h"

int retransmit(tloe_endpoint_t *e, int seq_num) {
    TloeEther *ether = (TloeEther *)e->fabric_ops.handle;
    CircularQueue *retransmit_buffer = e->retransmit_buffer;
    tloe_frame_t frame;
    int i, n;
    char send_buffer[MAX_BUFFER_SIZE];
    // retransmit 
    n = 0;
    for (i=retransmit_buffer->front; i != retransmit_buffer->rear; i = (i + 1) % retransmit_buffer->size) {
        RetransmitBufferElement *element = (RetransmitBufferElement *) retransmit_buffer->data[i];
        int diff = tloe_seqnum_cmp(element->tloe_frame.header.seq_num, seq_num);
        if (diff < 0)
            continue;

        frame = element->tloe_frame;
        
        tloe_seqnum_set_seq_num_ack(&frame, e);

        fprintf(stderr, "Retransmission with num_seq: %d\n", frame.header.seq_num);
        // Convert tloe_frame into packet
        tloe_frame_to_packet((tloe_frame_t *)&frame, send_buffer, element->f_size);
        tloe_fabric_send(e, send_buffer, element->f_size);

        element->state = TLOE_RESENT;
        element->send_time = get_current_timestamp(&(e->iteration_ts));
    }
    return n;
}

void slide_window(tloe_endpoint_t *e, tloe_frame_t *tloeframe) {
    TloeEther *ether = (TloeEther *)e->fabric_ops.handle;
    CircularQueue *retransmit_buffer = e->retransmit_buffer;
    RetransmitBufferElement *rbe;

    // dequeue TLoE frames from the retransmit buffer
    rbe = (RetransmitBufferElement *) getfront(retransmit_buffer);
    while (rbe != NULL) {
        int diff = tloe_seqnum_cmp(rbe->tloe_frame.header.seq_num, tloeframe->header.seq_num_ack);
        if (diff > 0)
            break;

        rbe = (RetransmitBufferElement *) dequeue(retransmit_buffer);
        //printf("RX: frame.last_seq_num: %d, element->seq_num: %d\n", last_seq_num, e->tloe_frame.seq_num);

        if (rbe) free(rbe);
        rbe = (RetransmitBufferElement *) getfront(retransmit_buffer);
    }
}

RetransmitBufferElement *get_earliest_element(CircularQueue *retransmit_buffer) {
    int i;
    for (i=retransmit_buffer->front; i != retransmit_buffer->rear; i = (i + 1) % retransmit_buffer->size) {
        RetransmitBufferElement *element = (RetransmitBufferElement *) retransmit_buffer->data[i];
        if (element->send_time == 0)
            continue;

        return element;
    }
    return NULL;
}

