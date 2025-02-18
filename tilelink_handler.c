#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tloe_frame.h"
#include "tloe_endpoint.h"
#include "tilelink_handler.h"
#include "util/circular_queue.h"

void tl_handler(tloe_endpoint_t *e) {
	// dequeue from th_buffer
	tl_msg_t *tlmsg;

	if (is_queue_empty(e->tl_msg_buffer))
		goto out;

	if (is_queue_full(e->response_buffer)) {
		//printf("%s reply buffer is full.\n", __func__);
		goto out;
	}

	tlmsg = (tl_msg_t *)dequeue(e->tl_msg_buffer); 

	// handle & make reponse
	switch (tlmsg->header.opcode) {
	case A_GET_OPCODE:
		tlmsg->header.chan = CHANNEL_D;
		tlmsg->header.opcode = C_ACCESSACKDATA_OPCODE;

		if (!enqueue(e->response_buffer, tlmsg)) {
			//printf("Failed to enqueue packet %d, buffer is full.\n", tloeframe->seq_num);
			e->drop_response_cnt++;
			free(tlmsg);
			goto out;
		}
		break;
	case C_ACCESSACKDATA_OPCODE:
		break;
	default:	
		//DEBUG
		printf("Unknown opcode. %d\n", tlmsg->header.opcode);
		exit(1);
	}

out:
}
#if 0
void tl_handler(TileLinkMsg *tl, int *channel, int *credit) {
	// Handling TileLink Msg

	// for testing
	*channel = 1;
	*credit = 2;

	// Send response
	TloeFrame *tloeframe = malloc(sizeof(TloeFrame));
	TileLinkMsg *tlmsg = malloc(sizeof(TileLinkMsg));
	memset(tloeframe, 0, sizeof(TloeFrame));

	if (tl->opcode == A_GET_OPCODE) {
		tloeframe->mask = 1;
		tloeframe->channel = CHANNEL_A;
		tloeframe->credit = 2;
		tloeframe->opcode = C_ACCESSACKDATA_OPCODE;

#if 1
		if (!enqueue(reply_buffer, tloeframe)) {
			printf("Failed to enqueue packet %d, buffer is full.\n", tloeframe->seq_num);
			goto out;
		}
#else
		free(tloeframe);
		goto out;
#endif

	} else if (tl->opcode == A_PUTFULLDATA_OPCODE) {
		exit(1);
		free(tlmsg);
		return;
	} else if (tl->opcode == C_ACCESSACKDATA_OPCODE) {
		exit(1);
		free(tlmsg);
		return;
	}
out:
	free(tlmsg);
}
#endif
