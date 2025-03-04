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

int get_tl_header_size(tl_msg_t *tlmsg) {
    int tl_chan = tlmsg->header.chan;
    int tl_opcode = tlmsg->header.opcode;
    int tl_size = tlmsg->header.size;
    int header_size = -1;

    switch (tl_chan) {
        case CHANNEL_A:
        case CHANNEL_B:
        case CHANNEL_C:
                header_size = 4;
            break;
        case CHANNEL_D:
            if (tl_opcode == D_ACCESSACK_OPCODE || tl_opcode == D_HINTACK_OPCODE ||
                    tl_opcode == D_RELEASEACK_OPCODE || tl_opcode == D_ACCESSACKDATA_OPCODE) {
                header_size = 3;
            } else if (tl_opcode == D_GRANT_OPCODE || tl_opcode == D_GRANTDATA_OPCODE) {
                header_size = 4;
            }
            break;
        case CHANNEL_E:
            // Channel E messages always consume a fixed amount of credit
            header_size = 3;
            break;
        default:
            // Handle invalid channels
            printf("Wrong Channel\n");
            exit(1);
    }

    // Header size 2^3 represents 8 bytes, and 4 represents 2^4, which is 16 bytes
    return header_size;
}

int get_tl_data_size(tl_msg_t *tlmsg) {
    int tl_chan = tlmsg->header.chan;
    int tl_opcode = tlmsg->header.opcode;
    int tl_size = tlmsg->header.size;
    int data_size = -1;

    switch (tl_chan) {
        case CHANNEL_A:
        case CHANNEL_B:
        case CHANNEL_C:
            if (tl_opcode == A_PUTFULLDATA_OPCODE || tl_opcode == B_PUTFULLDATA_OPCODE ||
                    tl_opcode == A_ARITHMETICDATA_OPCODE || tl_opcode == B_ARITHMETICDATA_OPCODE ||
                    tl_opcode == A_LOGICALDATA_OPCODE || tl_opcode == B_LOGICALDATA_OPCODE ||
                    tl_opcode == C_ACCESSACKDATA_OPCODE || tl_opcode == C_PROBEACKDATA_OPCODE ||
                    tl_opcode == C_RELEASEDATA_OPCODE || tl_opcode == A_PUTPARTIALDATA_OPCODE ||
                    tl_opcode == B_PUTPARTIALDATA_OPCODE)

                data_size = tl_size;
            break;
        case CHANNEL_D:
            if (tl_opcode == D_ACCESSACKDATA_OPCODE | tl_opcode == D_GRANTDATA_OPCODE)
                data_size = tl_size;
            break;
        case CHANNEL_E:
            break;
        default:
            // Handle invalid channels
            printf("Wrong Channel\n");
            exit(1);
    }

    return data_size;
}
