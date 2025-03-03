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

typedef enum {
    TLOE_FABRIC_QFLAGS_NONE=0,
    TLOE_FABRIC_QFLAGS_CREATE=1
} tloe_fabric_qflags_t;

// Generic function pointer types using void*
typedef size_t (*tloe_send_fn)(void *handle, char *data, size_t len);
typedef size_t (*tloe_recv_fn)(void *handle, char *data, size_t len);
typedef void* (*tloe_open_fn)(char *dev_name, char *dev_opt, int qflags);
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
    char *dev_opt;     // IP address for open
} tloe_fabric_ops_t;

// Forward declaration
struct tloe_endpoint_struct;

// Public functions
int tloe_fabric_init(struct tloe_endpoint_struct *, tloe_fabric_type_t);
size_t tloe_fabric_send(struct tloe_endpoint_struct *, char *data, size_t);
size_t tloe_fabric_recv(struct tloe_endpoint_struct *, char *data, size_t);
int tloe_fabric_open(struct tloe_endpoint_struct *, const char *, const char *, int);
void tloe_fabric_close(struct tloe_endpoint_struct *);

#endif /* TLOE_FABRIC_H */