#ifndef __TLOE_CONNECTION_H__
#define __TLOE_CONNECTION_H__
#include "tloe_endpoint.h"

#define CONN_PACKET_SIZE  56
#define CONN_RESEND_TIME  5000000
#define CONN_NUM_ACE      3

int open_conn_master(tloe_endpoint_t *e);
int open_conn_slave(tloe_endpoint_t *e);
int close_conn_master(tloe_endpoint_t *e);
int close_conn_slave(tloe_endpoint_t *e);
int is_conn(tloe_endpoint_t *e);

#endif //__TLOE_CONNECTION_H__
