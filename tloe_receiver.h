#ifndef __TLOE_RECEIVER_H__
#define __TLOE_RECEIVER_H__
#include "tloe_endpoint.h"

typedef struct {
	int channel;
	int credit;
} chan_credit_t;

void RX(tloe_endpoint_t *);

#endif // __TLOE_RECEIVER_H__
