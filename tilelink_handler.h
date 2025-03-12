#ifndef __TILELINK_HANDLER_H__
#define __TILELINK_HANDLER_H__

#include "tilelink_msg.h"

int tl_handler_init();
void tl_handler_close();
int tl_handler(tloe_endpoint_t *, int *, int*);

#endif // __TILELINK_HANDLER_H__
