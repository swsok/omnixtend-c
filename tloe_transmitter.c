#include <stdio.h>
#include <stddef.h>
#include "tloe_transmitter.h"
#include "tloe_endpoint.h"
#include "retransmission.h"
#include "timeout.h"

TloeFrame *TX(TloeFrame *tloeframe, tloe_endpoint_t *e) {
	TloeEther *ether = e->ether;
	TloeFrame *returnframe = NULL;
	RetransmitBufferElement *rbe;

	if (!is_queue_empty(e->ack_buffer)) {
		TloeFrame *ackframe = NULL;

		returnframe = tloeframe;
		ackframe = (TloeFrame *)dequeue(e->ack_buffer);

		// ACK/NAK (zero-TileLink)
		// Reflect the sequence number but do not store in the retransmission buffer, just send
		ackframe->seq_num = e->next_tx_seq;

		//printf("TX: Sending ACK/NAK with seq_num: %d, seq_num_ack: %d, ack: %d\n",
		//   tloeframe->seq_num, tloeframe->seq_num_ack, tloeframe->ack);

		tloe_ether_send(ether, (char *)ackframe, sizeof(TloeFrame));

		free(ackframe);
	} else if (tloeframe == NULL) {
	} else if (!is_queue_full(e->retransmit_buffer)) {
		// If the TileLink message is empty
		if (is_ack_msg(tloeframe)) {
			printf("ERROR: %s: %d\n", __FILE__, __LINE__);
			exit(1);
		}

		// NORMAL packet
		// Reflect the sequence number, store in the retransmission buffer, and send
		// Enqueue to retransmitBuffer
		rbe = (RetransmitBufferElement *)malloc(sizeof(RetransmitBufferElement));

		rbe->tloe_frame = *tloeframe;
		rbe->tloe_frame.seq_num = e->next_tx_seq;
		rbe->state = TLOE_INIT;
		rbe->send_time = get_current_time();

		if (!enqueue(e->retransmit_buffer, rbe)) {
			free(rbe);
			printf("ERROR: %s: %d\n", __FILE__, __LINE__);
			exit(1);
		}

		tloeframe->seq_num = e->next_tx_seq;
		tloeframe->seq_num_ack = e->acked_seq;
		tloeframe->ack = TLOE_ACK;
		tloeframe->mask = 1;

		// Update sequence number
		e->next_tx_seq = tloe_seqnum_next(e->next_tx_seq);

		//printf("TX: Sending TL msg with seq_num: %d, seq_num_ack: %d\n", tloeframe->seq_num, tloeframe->seq_num_ack);
		tloe_ether_send(ether, (char *)tloeframe, sizeof(TloeFrame));

		rbe->state = TLOE_SENT;
	} else {
		//printf("retransmit_buffer is full: %s: %d\n", __FILE__, __LINE__);
		returnframe = tloeframe;
	}

	// Timeout TX 
	rbe = getfront(e->retransmit_buffer);
    if (!rbe) return returnframe;

    if (is_timeout_tx(rbe->send_time)) {
        retransmit(e, rbe->tloe_frame.seq_num);
		printf("TX: Timeout TX and retranmission from seq_num: %d\n", rbe->tloe_frame.seq_num);
	}

    return returnframe;
}
