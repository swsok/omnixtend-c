#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tloe_frame.h"
#include "tloe_endpoint.h"
#include "tilelink_handler.h"
#include "util/circular_queue.h"

#define MEM_SIZE  (4ULL * 1024 * 1024 * 1024)     // 8GB

char *mem_storage = NULL;

int tl_handler_init() {
    int result = -1;
    if (mem_storage != NULL) {
        fprintf(stderr, "Memory is already initialized.\n");
        goto out;
    }

    mem_storage = (char *)malloc(MEM_SIZE);
    if (mem_storage == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        goto out;
    }

    memset(mem_storage, 0, MEM_SIZE);
out:
    return 0;
}

void tl_handler_close() {
    if (mem_storage != NULL) {
        free(mem_storage);
        mem_storage = NULL;
    } else {
        fprintf(stderr, "Memory is already freed or not initialized.\n");
    }
}

void handle_A_PUTFULLDATA_opcode(tloe_endpoint_t *e, tl_msg_t *tl) {
    // Write data to memory
    int data_size = 1 << (tl->header.size);
    int mem_offset = (tl->address) % MEM_SIZE;
    memcpy(mem_storage + mem_offset, tl->data, data_size);

    // Make tilelink response(AccessAck) and set data
	tl_msg_t *tlmsg = (tl_msg_t *)malloc(sizeof(tl_msg_t));	
	tlmsg->header.chan = CHANNEL_D;
	tlmsg->header.opcode = D_ACCESSACK_OPCODE;

    if (!enqueue(e->response_buffer, tlmsg)) {
		fprintf(stderr, "Failed to enqueue packet, buffer is full.\n");
		e->drop_response_cnt++;
		free(tlmsg);
    }
}

void handle_A_GET_opcode(tloe_endpoint_t *e, tl_msg_t *tl) {
    // Read data from memory
    int data_size = 1 << (tl->header.size);
    uint64_t *data = (uint64_t *)malloc(sizeof(uint64_t) * data_size);
    memcpy(data, mem_storage, data_size);

    // Make tilelink response(AccessAckData) and set data
	tl_msg_t *tlmsg = (tl_msg_t *)malloc(sizeof(tl_msg_t));	
	tlmsg->header.chan = CHANNEL_D;
	tlmsg->header.opcode = D_ACCESSACKDATA_OPCODE;

	if (!enqueue(e->response_buffer, tlmsg)) {
		fprintf(stderr, "Failed to enqueue packet, buffer is full.\n");
		e->drop_response_cnt++;
		free(data);
		free(tlmsg);
	}

    free(data);
}

void tl_handler(tloe_endpoint_t *e) {
    // Handling TileLink Msg
    tl_msg_t *tlmsg;

	if (is_queue_empty(e->tl_msg_buffer))
		goto out;

	if (is_queue_full(e->response_buffer)) {
		//printf("%s reply buffer is full.\n", __func__);
		goto out;
	}

	tlmsg = (tl_msg_t *)dequeue(e->tl_msg_buffer); 

    uint8_t tl_chan = tlmsg->header.chan;
    uint8_t tl_opcode = tlmsg->header.opcode;

	switch (tl_chan) {
	case CHANNEL_A:
		switch (tl_opcode) {
		case A_PUTFULLDATA_OPCODE:
            // return AccessAck
            handle_A_PUTFULLDATA_opcode(e, tlmsg);
			break;
		case A_PUTPARTIALDATA_OPCODE:
            // return AccessAck
            // to-be implemented
			break;
		case A_ARITHMETICDATA_OPCODE:
            // return AccessAckData
            // to-be implemented
			break;
		case A_LOGICALDATA_OPCODE:
            // return AccessAckData
            // to-be implemented
			break;
		case A_GET_OPCODE:
            // return AccessAckData
            handle_A_GET_opcode(e, tlmsg);
			break;
		case A_INTENT_OPCODE:
            // return HintAck
            // to-be implemented
			break;
		case A_ACQUIREBLOCK_OPCODE:
            // return Grant or GrantData
            // to-be implemented
			break;
		case A_ACQUIREPERM_OPCODE:
            // return Grant
            // to-be implemented
			break;
		}
		break;
	case CHANNEL_B:
		switch (tl_opcode) {
		case B_PUTFULLDATA_OPCODE:
            // return AccessAck
            // to-be implemented
			break;
		case B_PUTPARTIALDATA_OPCODE:
            // return AccessAck
            // to-be implemented
			break;
		case B_ARITHMETICDATA_OPCODE:
            // return AccessAckData
            // to-be implemented
			break;
		case B_LOGICALDATA_OPCODE:
            // return AccessAckData
            // to-be implemented
			break;
		case B_GET_OPCODE:
            // return AccessAckData
            // to-be implemented
			break;
		case B_INTENT_OPCODE:
            // return HintAck
            // to-be implemented
			break;
		case B_PROBEBLOCK_OPCODE:
            // return ProbeAck or ProbeAckData
            // to-be implemented
			break;
		case B_PROBEPERM_OPCODE:
            // return ProbeAck
            // to-be implemented
			break;
		}
		break;
	case CHANNEL_C:
		switch (tl_opcode) {
		case C_ACCESSACK_OPCODE:
            // return
			break;
		case C_ACCESSACKDATA_OPCODE:
            // return
			break;
		case C_HINTACK_OPCODE:
            // return
            // to-be implemented
			break;
		case C_PROBEACK_OPCODE:
            // return
            // to-be implemented
			break;
		case C_PROBEACKDATA_OPCODE:
            // return
            // to-be implemented
			break;
		case C_RELEASE_OPCODE:
            // return ReleaseAck
            // to-be implemented
			break;
		case C_RELEASEDATA_OPCODE:
            // return ReleaseAck
            // to-be implemented
			break;
		}
		break;
	case CHANNEL_D:
		switch (tl_opcode) {
		case D_ACCESSACK_OPCODE:
            // return
            // to-be implemented
			break;
		case D_ACCESSACKDATA_OPCODE:
            // return
            // to-be implemented
			break;
		case D_HINTACK_OPCODE:
            // return
            // to-be implemented
			break;
		case D_GRANT_OPCODE:
            // return
            // to-be implemented
			break;
		case D_GRANTDATA_OPCODE:
            // return
            // to-be implemented
			break;
		case D_RELEASEACK_OPCODE:
            // return
            // to-be implemented
			break;
		}		
		break;
	case CHANNEL_E:
		switch (tl_opcode) {
		case E_GRANTACK:
            // return
            // to-be implemented
			break;
		}
		break;
	default:
		//DEBUG
		printf("Unknown channel or opcode. %d/%d\n", tl_chan, tl_opcode);
		exit(1);
	}

	free(tlmsg);
out:
}

