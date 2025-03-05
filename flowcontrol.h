#ifndef __FLOWCONTROL_H__
#define __FLOWCONTROL_H__
#include "tloe_frame.h"
#include "tloe_ether.h"

#define CONN_OPEN       1
#define CONN_CLOSE      0
#define CREDIT_INIT     7
#define CREDIT_DEFAULT  10

typedef struct {
    unsigned int credits[CHANNEL_NUM];
} flowcontrol_t;

void init_flowcontrol(flowcontrol_t *); 
void set_credit(flowcontrol_t *, int, int);
int is_filled_credit(flowcontrol_t *, int);
int check_all_channels(flowcontrol_t *);
int get_credit(flowcontrol_t *fc, const int channel);
int cal_flits(tl_msg_t *);
int fc_credit_inc(flowcontrol_t *, tloe_frame_t *);
int fc_credit_dec(flowcontrol_t *, tl_msg_t *);
void fc_credit_print(flowcontrol_t *fc);

#endif // __FLOWCONTROL_H__
