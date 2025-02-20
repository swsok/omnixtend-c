#ifndef TLOE_FABRIC_H
#define TLOE_FABRIC_H

#include <stdint.h>
#include <stddef.h>
#include "tloe_ether.h"
#include "tloe_mq.h"

// Media type definition
typedef enum {
    TLOE_FABRIC_ETHER,
    TLOE_FABRIC_MQ
} tloe_fabric_type_t;

// Generic function pointer types using void*
typedef size_t (*tloe_send_fn)(void *handle, char *data, size_t len);
typedef size_t (*tloe_recv_fn)(void *handle, char *data, size_t len);
typedef void* (*tloe_open_fn)(char *dev_name, char *ip_addr);
typedef void (*tloe_close_fn)(void *handle);

// Media interface structure
typedef struct {
    tloe_send_fn send;
    tloe_recv_fn recv;
    tloe_open_fn open;
    tloe_close_fn close;
    void *handle;
    tloe_fabric_type_t type;
    char *dev_name;    // Device name for open
    char *ip_addr;     // IP address for open
} tloe_fabric_ops_t;

// Forward declaration
struct tloe_endpoint_struct;

// Public functions
int tloe_fabric_init(struct tloe_endpoint_struct *ep, tloe_fabric_type_t media_type);
size_t tloe_fabric_send(struct tloe_endpoint_struct *ep, char *data, size_t len);
size_t tloe_fabric_recv(struct tloe_endpoint_struct *ep, char *data, size_t len);
int tloe_fabric_open(struct tloe_endpoint_struct *ep, const char *dev_name, const char *ip_addr);
void tloe_fabric_close(struct tloe_endpoint_struct *ep);

#endif /* TLOE_FABRIC_H */