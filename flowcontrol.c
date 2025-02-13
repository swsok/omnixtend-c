#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "flowcontrol.h"
#include "tilelink_msg.h"

void init_flowcontrol(flowcontrol_t *fc) {
	for (int i=0; i<CHANNEL_NUM; i++) 
		fc->credits[i] = 0;	
}

void set_credit(flowcontrol_t *fc, int channel, int credit) {
    fc->credits[channel] = (1 << credit);
}

int check_all_channels(flowcontrol_t *fc) {
    int result = 0;
    printf("Wait until all channels have their credits set.....\n");

    while(!(fc->credits[CHANNEL_A] && \
        fc->credits[CHANNEL_B] && \
        fc->credits[CHANNEL_C] && \
        fc->credits[CHANNEL_D] && \
        fc->credits[CHANNEL_E])){
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

#if 0
flowcontrol_t *create_credit() {
    flowcontrol_t *fc = (flowcontrol_t *) malloc (sizeof(flowcontrol_t));
    memset(fc, 0, sizeof(flowcontrol_t));

    return fc;
}

int get_credit(flowcontrol_t *fc, const int channel) {
    return fc->available_credits[channel];
}

#endif
