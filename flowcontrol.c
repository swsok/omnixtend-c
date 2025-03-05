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
    fc->credits[channel] += (1 << credit);
}

int is_filled_credit(flowcontrol_t *fc, int channel) {
#if 0 // WD only returns A, C, and E channels 
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

#if 0 // WD only returns A, C, and E channels 
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

// Function to decrease credits if sufficient, otherwise return -1
int try_dec_credits(flowcontrol_t *fc, int tl_chan, int amount) {
    int result;
    if (fc->credits[tl_chan] < amount) {
        result = -1;  // Not enough credit
    } else {
        fc->credits[tl_chan] -= amount;
        result = fc->credits[tl_chan];  // Return remaining credit
    }    

    return result;
}

int cal_flits(tl_msg_t *tlmsg) {
    int header_size = tlmsg_get_header_size(tlmsg);
    int data_size = tlmsg_get_data_size(tlmsg);

    int total_flits = 0;

    // Calculate required total flits: Header + Data
    if (header_size > 0)
        total_flits += ((1ULL << header_size) + 7 ) / 8;
    if (data_size > 0)
        total_flits += ((1ULL << data_size) + 7 ) / 8;

    return total_flits;
}

int fc_credit_inc(flowcontrol_t *fc, tloe_frame_t *tloeframe) {
    int inc_credit = -1;
    
    if (tloeframe->header.chan != 0) { 
        inc_credit = (1 << tloeframe->header.credit);
        fc->credits[tloeframe->header.chan] += inc_credit;
    }

    return inc_credit;
}

int fc_credit_dec(flowcontrol_t *fc, tl_msg_t *tlmsg) {
    int tl_chan = tlmsg->header.chan;
    int available_credit;
    int dec_credit = cal_flits(tlmsg);

    if ((available_credit = try_dec_credits(fc, tl_chan, dec_credit)) == -1) 
        dec_credit = -1; 

    return dec_credit;
}

int get_credit(flowcontrol_t *fc, const int channel) {
    return fc->credits[channel];
}
