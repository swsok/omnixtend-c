#ifndef __TLOE_CONNECTION_H__
#define __TLOE_CONNECTION_H__
#include "tloe_endpoint.h"

void open_conn_master(tloe_endpoint_t *e);
void open_conn_slave(tloe_endpoint_t *e);
void close_conn_master(tloe_endpoint_t *e);
void close_conn_slave(tloe_endpoint_t *e);
int is_conn(tloe_endpoint_t *e);

#endif //__TLOE_CONNECTION_H__
