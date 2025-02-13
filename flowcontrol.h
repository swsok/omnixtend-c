#ifndef __FLOWCONTROL_H__
#define __FLOWCONTROL_H__
#include "tloe_frame.h"

#define DEFAULT_CREDIT  10

typedef struct {
    unsigned int credits[CHANNEL_NUM];
} flowcontrol_t;

void init_flowcontrol(flowcontrol_t *); 
void set_credit(flowcontrol_t *, int, int);
int check_all_channels(flowcontrol_t *);
void inc_credit(flowcontrol_t *, int, int);
int dec_credit(flowcontrol_t *, int, int);
#if 0
flowcontrol_t *create_credit();
int get_credit(flowcontrol_t *fc, const int channel);
#endif

#endif // __FLOWCONTROL_H__
