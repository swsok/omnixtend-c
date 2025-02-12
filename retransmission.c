#include "retransmission.h"
#include "tloe_common.h"
#include "timeout.h"

int retransmit(TloeEther *ether, CircularQueue *retransmit_buffer, int seq_num) {
	int i, n;
	TloeFrame frame;
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
		element->send_time = get_current_time();
	}
	return n;
}

void slide_window(TloeEther *ether, CircularQueue *retransmit_buffer, int last_seq_num) {
    RetransmitBufferElement *e;

    // dequeue TLoE frames from the retransmit buffer
    e = (RetransmitBufferElement *) getfront(retransmit_buffer);
    while (e != NULL) {
		int diff = tloe_seqnum_cmp(e->tloe_frame.seq_num, last_seq_num);
	    if (diff > 0)
		    break;

        e = (RetransmitBufferElement *) dequeue(retransmit_buffer);
		//printf("RX: frame.last_seq_num: %d, element->seq_num: %d\n", last_seq_num, e->tloe_frame.seq_num);
        if (e) free(e);
        e = (RetransmitBufferElement *) getfront(retransmit_buffer);
    }
}
