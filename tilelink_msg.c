#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tilelink_msg.h"

int get_tlmsg_credit(tl_msg_t *tlmsg) {
	return 1;
}

int tlmsg_get_chan(tl_msg_t *tlmsg) {
    return tlmsg->header.chan;
}

int tlmsg_get_opcode(tl_msg_t *tlmsg) {
    return tlmsg->header.opcode;
}

int tlmsg_get_size(tl_msg_t *tlmsg) {
    return tlmsg->header.size;
}
// Operations for TileLink messages
