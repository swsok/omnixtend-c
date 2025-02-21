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
    return fc->credits[channel] > (1 << CREDIT_INIT);
}

int check_all_channels(flowcontrol_t *fc) {
    int result = 1;
    int credit_init = (1 << CREDIT_INIT);

    if ((fc->credits[CHANNEL_A] > credit_init) && \
        (fc->credits[CHANNEL_B] > credit_init) && \
        (fc->credits[CHANNEL_C] > credit_init) && \
        (fc->credits[CHANNEL_D] > credit_init) && \
        (fc->credits[CHANNEL_E] > credit_init)) {
        result = 0;
    }

    return result;
}

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

int get_credit(flowcontrol_t *fc, const int channel) {
    return fc->credits[channel];
}
