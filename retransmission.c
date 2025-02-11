#include "retransmission.h"
#include "tloe_common.h"

int retransmit(TloeEther *ether, CircularQueue *retransmit_buffer, int seq_num) {
	int i, n;
	TloeFrame frame;
	// retransmit 
	n = 0;
	for (i=retransmit_buffer->front; i != retransmit_buffer->rear; i = (i + 1) % retransmit_buffer->size) {
		RetransmitBufferElement *element = (RetransmitBufferElement *) retransmit_buffer->data[i];
		int diff = seq_num_diff(element->tloe_frame.seq_num, seq_num);
		if (diff < ((MAX_SEQ_NUM + 1) / 2))
			continue;

		frame = element->tloe_frame;
		frame.mask = 1;		// Indicate to normal packet

		printf("Retransmission with num_seq: %d\n", frame.seq_num);
		//			tloe_ether_send(ether, (char *)&frame, sizeof(TloeFrame));
		element->state = TLOE_RESENT;
		element->send_time = time(NULL);
	}
	return n;
}

int slide_window(TloeEther *ether, CircularQueue *retransmit_buffer, int seq_num_ack) {
    RetransmitBufferElement *e;
	int acked_seq = 0;

    // dequeue TLoE frames from the retransmit buffer
    e = (RetransmitBufferElement *) getfront(retransmit_buffer);
    while (e != NULL) {
	    int diff = seq_num_diff(e->tloe_frame.seq_num, seq_num_ack);
	    if (diff >= ((MAX_SEQ_NUM + 1) / 2))
		    break;

        e = (RetransmitBufferElement *) dequeue(retransmit_buffer);
		//printf("RX: frame.seq_num_ack: %d, element->seq_num: %d\n", seq_num_ack, e->tloe_frame.seq_num);
        acked_seq = e->tloe_frame.seq_num;
        if (e) free(e);
        e = (RetransmitBufferElement *) getfront(retransmit_buffer);
    }
	return acked_seq;
}
