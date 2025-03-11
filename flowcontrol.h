#ifndef __FLOWCONTROL_H__
#define __FLOWCONTROL_H__
#include "tloe_ether.h"
#include "tloe_frame.h"

#define CONN_OPEN 1
#define CONN_CLOSE 0
#define CREDIT_INIT 7
#define CREDIT_DEFAULT 10

typedef struct {
    unsigned int credits[CHANNEL_NUM];
    unsigned int tx_flow_credits[CHANNEL_NUM];

    int inc_cnt[CHANNEL_NUM];
    int inc_value[CHANNEL_NUM];
    int dec_cnt[CHANNEL_NUM];
    int dec_value[CHANNEL_NUM];
} flowcontrol_t;

void init_flowcontrol(flowcontrol_t *);
void set_credit(flowcontrol_t *, int, int);
int is_filled_credit(flowcontrol_t *, int);
int check_all_channels(flowcontrol_t *);
int get_credit(flowcontrol_t *, const int);
int cal_flits(tl_msg_t *);
int fc_credit_inc(flowcontrol_t *, tloe_frame_t *);
int fc_credit_dec(flowcontrol_t *, tl_msg_t *);
void fc_credit_print(flowcontrol_t *);
int fc_credit_update(flowcontrol_t *, tloe_frame_t *);
int add_channel_flow_credits(flowcontrol_t *, int, int);
unsigned int get_outgoing_credits(flowcontrol_t *, int);

static inline int select_max_credit_channel(flowcontrol_t *fc) {
    int i;
    unsigned int max_credit = fc->tx_flow_credits[0];
    int chan = 0;

    for (i = 1; i < CHANNEL_NUM; i++) {
        if (fc->tx_flow_credits[i] <= max_credit)
            continue;
        max_credit = fc->tx_flow_credits[i];
        chan = i;
    }   
    return chan;
}

static inline int get_largest_pow(unsigned int value) {
    return 31 - __builtin_clz(value);
}

#endif // __FLOWCONTROL_H__
