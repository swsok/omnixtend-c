#ifndef __TLOE_TRANSMITTER_H__
#define __TLOE_TRANSMITTER_H__
#include "tloe_frame.h"
#include "tloe_endpoint.h"

void open_conn(tloe_endpoint_t *);
tl_msg_t *TX(tloe_endpoint_t *, tl_msg_t *);

#endif // __TLOE_TRANSMITTER_H__
