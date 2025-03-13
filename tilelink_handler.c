#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tloe_frame.h"
#include "tloe_endpoint.h"
#include "tloe_common.h"
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

// Function prototype for handler functions
typedef void (*tl_handler_fn)(tloe_endpoint_t *e, tl_msg_t *tl);

void handle_A_PUTFULLDATA_opcode(tloe_endpoint_t *e, tl_msg_t *tl) {
    // Write data to memory
    int data_size = 1 << (tl->header.size);
    uint64_t mem_offset = ((uint64_t)(tl->data[0]) % MEM_SIZE);
    BUG_ON((mem_offset + data_size) > MEM_SIZE, "TL_Handler: Memory access out of bounds\n");
    memcpy(mem_storage + mem_offset, &(tl->data[1]), data_size);

    // Make tilelink response(AccessAck) and set data
    tl_msg_t *tlmsg = (tl_msg_t *)malloc(sizeof(tl_msg_t));	
    memset((void *)tlmsg, 0, sizeof(tl_msg_t));

    tlmsg->header.chan = CHANNEL_D;
    tlmsg->header.opcode = D_ACCESSACK_OPCODE;
    tlmsg->header.size = 0;
    tlmsg->header.source = tl->header.source;

    if (!enqueue(e->response_buffer, tlmsg)) {
        fprintf(stderr, "Failed to enqueue packet, buffer is full.\n");
        e->drop_response_cnt++;
        free(tlmsg);
    }
#ifdef DEBUG
    printf("%s Data received!\n", __func__);
#endif
}

void handle_A_GET_opcode(tloe_endpoint_t *e, tl_msg_t *tl) {
    // Read data from memory
    int data_size = 1 << (tl->header.size);
    uint64_t mem_offset = ((uint64_t)(tl->data[0]) % MEM_SIZE);
    BUG_ON((mem_offset + data_size) > MEM_SIZE, "TL_Handler: Memory access out of bounds\n");

    // Make tilelink response(AccessAckData) and set data
    tl_msg_t *tlmsg = (tl_msg_t *)malloc(sizeof(tl_msg_t) + data_size);	
    memset((void *)tlmsg, 0, sizeof(tl_msg_t) + data_size);

    tlmsg->header.chan = CHANNEL_D;
    tlmsg->header.opcode = D_ACCESSACKDATA_OPCODE;
    tlmsg->header.size = tl->header.size;
    memcpy(&(tlmsg->data[0]), mem_storage + mem_offset, data_size);
    tlmsg->header.source = tl->header.source;

    if (!enqueue(e->response_buffer, tlmsg)) {
        fprintf(stderr, "Failed to enqueue packet, buffer is full.\n");
        e->drop_response_cnt++;
        free(tlmsg);
    }
#ifdef DEBUG
    printf("%s Data received!\n", __func__);
#endif
}

void handle_D_ACCESSACK_opcode(tloe_endpoint_t *e, tl_msg_t *tl) {
    // Do nothing TODO
    e->accessack_cnt++;
}

void handle_D_ACCESSACKDATA_opcode(tloe_endpoint_t *e, tl_msg_t *tl) {
    uint64_t result = 0;
    int size = (((1ULL << tl->header.size) + 7 ) / 8);

    result = (uint64_t)tl->data[0];

    printf("0x%lX\n", result);

    e->accessackdata_cnt++;
}

static void handle_null_opcode(tloe_endpoint_t *e, tl_msg_t *tl) {
}

static void handle_debug_opcode(tloe_endpoint_t *e, tl_msg_t *tl) {
    printf("DEBUG: Unimplemented handler called - Channel: %d, Opcode: %d\n", 
           tl->header.chan, tl->header.opcode);
}

// Handler table declaration
tl_handler_fn tl_handler_table[CHANNEL_NUM][TL_OPCODE_NUM] = {
	// Channel 0
    {
        handle_debug_opcode,
        handle_debug_opcode,
        handle_debug_opcode,
        handle_debug_opcode,
        handle_debug_opcode,
        handle_debug_opcode,
        handle_debug_opcode,
        handle_debug_opcode,
    },
	// Channel A
    {
        handle_A_PUTFULLDATA_opcode,  // A_PUTFULLDATA
        handle_debug_opcode,          // A_PUTPARTIALDATA
        handle_debug_opcode,          // A_ARITHMETICDATA
        handle_debug_opcode,          // A_LOGICALDATA
        handle_A_GET_opcode,          // A_GET
        handle_debug_opcode,          // A_INTENT
        handle_debug_opcode,          // NOT USED
        handle_debug_opcode,          // NOT USED
    },
	// Channel B
    {
        handle_debug_opcode,          // B_PUTFULLDATA
        handle_debug_opcode,          // B_PUTPARTIALDATA
        handle_debug_opcode,          // B_ARITHMETICDATA
        handle_debug_opcode,          // B_LOGICALDATA
        handle_debug_opcode,          // B_GET
        handle_debug_opcode,          // B_INTENT
        handle_debug_opcode,          // B_PROBEBLOCK
        handle_debug_opcode,          // B_PROBEPERM
    },
	// Channel C
    {
        handle_debug_opcode,          // C_ACCESSACK
        handle_debug_opcode,          // C_ACCESSACKDATA
        handle_debug_opcode,          // C_HINTACK
        handle_debug_opcode,          // C_PROBEACK
        handle_debug_opcode,          // C_PROBEACKDATA
        handle_debug_opcode,          // C_RELEASE
        handle_debug_opcode,          // C_RELEASEDATA
        handle_debug_opcode,          // NOT USED
    },
	// Channel D
    {
        handle_D_ACCESSACK_opcode,      // D_ACCESSACK
        handle_D_ACCESSACKDATA_opcode,  // D_ACCESSACKDATA
        handle_debug_opcode,            // D_HINTACK
        handle_debug_opcode,            // D_GRANT
        handle_debug_opcode,            // D_GRANTDATA
        handle_debug_opcode,            // D_RELEASEACK
        handle_debug_opcode,            // NOT USED
        handle_debug_opcode,            // NOT USED
    },
	// Channel E
    {
        handle_debug_opcode,          // E_GRANTACK
        handle_debug_opcode,          // NOT USED
        handle_debug_opcode,          // NOT USED
        handle_debug_opcode,          // NOT USED
        handle_debug_opcode,          // NOT USED
        handle_debug_opcode,          // NOT USED
        handle_debug_opcode,          // NOT USED
        handle_debug_opcode,          // NOT USED
    },
};

int tl_handler(tloe_endpoint_t *e, int *chan, int *credit) {
    tl_msg_t *tlmsg = NULL;
    int ret;
    uint8_t tl_chan;
    uint8_t tl_opcode;

    // Skip processing if TileLink message buffer is empty or response buffer is full
    if (is_queue_empty(e->tl_msg_buffer) || is_queue_full(e->response_buffer)) {
	ret = 0;
        goto out;
    }

    // Get next message from the message buffer
    tlmsg = (tl_msg_t *)dequeue(e->tl_msg_buffer); 

    // Extract channel and opcode information from message header
    tl_chan = tlmsg->header.chan;
    tl_opcode = tlmsg->header.opcode;

    *chan = tlmsg->header.chan;
    *credit = tlmsg_get_flits_cnt(tlmsg);

    // Process message by calling the appropriate handler function based on channel and opcode
    tl_handler_table[tl_chan][tl_opcode](e, tlmsg);

    free(tlmsg);
    ret = 1;
out:
    return ret;
}

