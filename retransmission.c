#include "retransmission.h"
#include "tloe_endpoint.h"
#include "tloe_common.h"

int retransmit(tloe_endpoint_t *e, int seq_num) {
	TloeEther *ether = e->ether;
	CircularQueue *retransmit_buffer = e->retransmit_buffer;
	TloeFrame frame;
	int i, n;
	// retransmit 
	n = 0;
	for (i=retransmit_buffer->front; i != retransmit_buffer->rear; i = (i + 1) % retransmit_buffer->size) {
		RetransmitBufferElement *element = (RetransmitBufferElement *) retransmit_buffer->data[i];
		int diff = tloe_seqnum_cmp(element->tloe_frame.seq_num, seq_num);
		if (diff < 0)
			continue;

		frame = element->tloe_frame;
		frame.mask = 1;		// Indicate to normal packet

		printf("Retransmission with num_seq: %d\n", frame.seq_num);
		tloe_ether_send(ether, (char *)&frame, sizeof(TloeFrame));

		element->state = TLOE_RESENT;
		element->send_time = get_current_timestamp(&(e->iteration_ts));
	}
	return n;
}

void slide_window(tloe_endpoint_t *e, int last_seq_num) {
	TloeEther *ether = e->ether;
	CircularQueue *retransmit_buffer = e->retransmit_buffer;
    RetransmitBufferElement *rbe;

    // dequeue TLoE frames from the retransmit buffer
    rbe = (RetransmitBufferElement *) getfront(retransmit_buffer);
    while (rbe != NULL) {
		int diff = tloe_seqnum_cmp(rbe->tloe_frame.seq_num, last_seq_num);
	    if (diff > 0)
		    break;

        rbe = (RetransmitBufferElement *) dequeue(retransmit_buffer);
		//printf("RX: frame.last_seq_num: %d, element->seq_num: %d\n", last_seq_num, e->tloe_frame.seq_num);
        if (rbe) free(rbe);
        rbe = (RetransmitBufferElement *) getfront(retransmit_buffer);
    }
}
