#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "flowcontrol.h"
#include "tilelink_msg.h"
#include "tloe_frame.h"

void init_flowcontrol(flowcontrol_t *fc) {
	for (int i=0; i<CHANNEL_NUM; i++) 
		set_credit(fc, i, CREDIT_INIT);	
}

void set_credit(flowcontrol_t *fc, int channel, int credit) {
    fc->credits[channel] = (1 << credit);
}

int is_filled_credit(flowcontrol_t *fc, int channel) {
#if 0
    return fc->credits[channel] > (1 << CREDIT_INIT);
#else
    if (channel == CHANNEL_A || channel == CHANNEL_C || channel == CHANNEL_E)
        return fc->credits[channel] > (1 << CREDIT_INIT);
    else
        return 0;
#endif
}

int check_all_channels(flowcontrol_t *fc) {
    int result = 1;
    int credit_init = (1 << CREDIT_INIT);

#if 0
    if ((fc->credits[CHANNEL_A] > credit_init) && \
        (fc->credits[CHANNEL_B] > credit_init) && \
        (fc->credits[CHANNEL_C] > credit_init) && \
        (fc->credits[CHANNEL_D] > credit_init) && \
        (fc->credits[CHANNEL_E] > credit_init)) {
#else
    if ((fc->credits[CHANNEL_A] > credit_init) && \
        (fc->credits[CHANNEL_C] > credit_init) && \
        (fc->credits[CHANNEL_E] > credit_init)) {
#endif
        result = 0;
    }

    return result;
}

#if 0
void inc_credit(flowcontrol_t *fc, int channel, int credit) {
    unsigned int flit_cnt = (1 << credit);

    fc->credits[channel] += flit_cnt;
}

int dec_credit(flowcontrol_t *fc, int channel, int credit) {
    int result = 0;
    unsigned int flit_cnt = (1 << credit);   // Should be modified depending on tlmsg's opcode		

    if (fc->credits[channel] >= flit_cnt) {
        fc->credits[channel] -= flit_cnt;
        result = 1;
    }

    return result;
}
#endif

int get_tl_header_size(tl_msg_t *tlmsg) {
    int tl_chan = tlmsg->header.chan;
    int tl_opcode = tlmsg->header.opcode;
    int tl_size = tlmsg->header.size;
    int header_size = -1;

    switch (tl_chan) {
        case CHANNEL_A:
        case CHANNEL_B:
        case CHANNEL_C:
                header_size = 2;
            break;
        case CHANNEL_D:
            if (tl_opcode == D_ACCESSACK_OPCODE || tl_opcode == D_HINTACK_OPCODE ||
                    tl_opcode == D_RELEASEACK_OPCODE || tl_opcode == D_ACCESSACKDATA_OPCODE) {
                header_size = 1;
            } else if (tl_opcode == D_GRANT_OPCODE || tl_opcode == D_GRANTDATA_OPCODE) {
                header_size = 2;
            }
            break;
        case CHANNEL_E:
            // Channel E messages always consume a fixed amount of credit
            header_size = 1;
            break;
        default:
            // Handle invalid channels
            printf("Wrong Channel\n");
            exit(1);
    }

    return header_size == (header_size == 1) ? 3 : (header_size == 2) ? 4 : -1;
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

int credit_inc(flowcontrol_t *fc, int tl_chan, int amount) {
    fc->credits[tl_chan] += amount;

    return fc->credits[tl_chan];
}

// Function to decrease credits if sufficient, otherwise return -1
int try_dec_credits(flowcontrol_t *fc, int tl_chan, int amount) {
    int result = 0;
    if (fc->credits[tl_chan] < amount) {
        result = -1;  // Not enough credit
    } else {
        fc->credits[tl_chan] -= amount;
        result = fc->credits[tl_chan];  // Return remaining credit
    }    

    return result;
}

int credit_dec(flowcontrol_t *fc, tl_msg_t *tlmsg) {
    int tl_chan = tlmsg->header.chan;
    int tl_opcode = tlmsg->header.opcode;
    int tl_size = tlmsg->header.size;
    int available_credit = -1;

    int header_size = get_tl_header_size(tlmsg);
    int data_size = get_tl_data_size(tlmsg);

    int total_flits = 0;

    if (header_size > 0)
        total_flits += ((1ULL << header_size) + 7 ) / 8;
    if (data_size > 0)
        total_flits += ((1ULL << data_size) + 7 ) / 8;

    available_credit = try_dec_credits(fc, tl_chan, total_flits);

    return available_credit;
#if 0
    int tl_chan = tlmsg->header.chan;
    int tl_opcode = tlmsg->header.opcode;
    int tl_size = tlmsg->header.size;
    int result = -1;

    switch (tl_chan) {
        case CHANNEL_A:
        case CHANNEL_B:
        case CHANNEL_C:
            // Decrease header size (1 << 1) = 2
            if ((result = try_dec_credits(fc, tl_chan, 2)) == -1)
                goto out;

            // Decrease data size if required
            if (tl_opcode == A_PUTFULLDATA_OPCODE || tl_opcode == B_PUTFULLDATA_OPCODE ||
                    tl_opcode == A_ARITHMETICDATA_OPCODE || tl_opcode == B_ARITHMETICDATA_OPCODE ||
                    tl_opcode == A_LOGICALDATA_OPCODE || tl_opcode == B_LOGICALDATA_OPCODE ||
                    tl_opcode == C_ACCESSACKDATA_OPCODE || tl_opcode == C_PROBEACKDATA_OPCODE ||
                    tl_opcode == C_RELEASEDATA_OPCODE) { 

                result = try_dec_credits(fc, tl_chan, 1 << tl_size);
            } else if (tl_opcode == A_PUTPARTIALDATA_OPCODE || tl_opcode == B_PUTPARTIALDATA_OPCODE) {
                result = try_dec_credits(fc, tl_chan, 1 << tl_size);
            }
            break;

        case CHANNEL_D:
            if (tl_opcode == D_ACCESSACK_OPCODE || tl_opcode == D_HINTACK_OPCODE ||
                    tl_opcode == D_RELEASEACK_OPCODE || tl_opcode == D_ACCESSACKDATA_OPCODE) {
                // Decrease header size (1 << 0) = 1
                if ((result = try_dec_credits(fc, tl_chan, 1)) == -1)
                    goto out;

                // Decrease data size if required
                if (tl_opcode == D_ACCESSACKDATA_OPCODE)
                    result = try_dec_credits(fc, tl_chan, 1 << tl_size);
            } else if (tl_opcode == D_GRANT_OPCODE || tl_opcode == D_GRANTDATA_OPCODE) {
                // Decrease header size (1 << 1) = 2
                if ((result = try_dec_credits(fc, tl_chan, 2)) == -1)
                    goto out;

                // Decrease data size if required
                if (tl_opcode == D_GRANTDATA_OPCODE)
                    result = try_dec_credits(fc, tl_chan, 1 << tl_size);
            }
            break;

        case CHANNEL_E:
            // Channel E messages always consume a fixed amount of credit
            result = try_dec_credits(fc, tl_chan, 1);
            break;

        default:
            // Handle invalid channels
            printf("Wrong Channel\n");
            exit(1);
    }

out:
    return result;
#endif
}

int get_credit(flowcontrol_t *fc, const int channel) {
    return fc->credits[channel];
}
