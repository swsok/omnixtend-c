#include "tloe_fabric.h"
#include "tloe_ether.h"
#include "tloe_mq.h"
#include "tloe_endpoint.h"

// Wrapper functions for Ethernet
static size_t ether_send_wrapper(void *handle, char *data, size_t len) {
    return tloe_ether_send((struct tloe_ether_struct *)handle, data, len);
}

static size_t ether_recv_wrapper(void *handle, char *data, size_t len) {
    return tloe_ether_recv((struct tloe_ether_struct *)handle, data, len);
}

static void *ether_open_wrapper(char *dev_name, char *dev_opt, int create_queue) {
    return tloe_ether_open(dev_name, dev_opt, create_queue);
}

static void ether_close_wrapper(void *handle) {
    tloe_ether_close((struct tloe_ether_struct *)handle);
}

// Wrapper functions for MQ
static size_t mq_send_wrapper(void *handle, char *data, size_t len) {
    return tloe_mq_send((struct tloe_mq_struct *)handle, data, len);
}

static size_t mq_recv_wrapper(void *handle, char *data, size_t len) {
    return tloe_mq_recv((struct tloe_mq_struct *)handle, data, len);
}

static void *mq_open_wrapper(char *dev_name, char *dev_opt, int create_queue) {
    return tloe_mq_open(dev_name, dev_opt, create_queue);
}

static void mq_close_wrapper(void *handle) {
    tloe_mq_close((struct tloe_mq_struct *)handle);
}

// Ethernet media operations
static tloe_fabric_ops_t ether_ops = {
    .send = ether_send_wrapper,
    .recv = ether_recv_wrapper,
    .open = ether_open_wrapper,
    .close = ether_close_wrapper,
    .handle = NULL,
    .type = TLOE_FABRIC_ETHER,
    .dev_name = NULL,
    .dev_opt = NULL
};

// Message Queue media operations
static tloe_fabric_ops_t mq_ops = {
    .send = mq_send_wrapper,
    .recv = mq_recv_wrapper,
    .open = mq_open_wrapper,
    .close = mq_close_wrapper,
    .handle = NULL,
    .type = TLOE_FABRIC_MQ,
    .dev_name = NULL,
    .dev_opt = NULL
};

int tloe_fabric_init(tloe_endpoint_t *ep, tloe_fabric_type_t media_type) {
    switch (media_type) {
        case TLOE_FABRIC_ETHER:
            ep->fabric_ops = ether_ops;
            break;
        case TLOE_FABRIC_MQ:
            ep->fabric_ops = mq_ops;
            break;
        default:
            return -1;
    }
    return 0;
}

size_t tloe_fabric_send(tloe_endpoint_t *ep, char *data, size_t len) {
    if (ep->fabric_ops.send == NULL || ep->fabric_ops.handle == NULL) {
        return -1;
    }
    return ep->fabric_ops.send(ep->fabric_ops.handle, data, len);
}

size_t tloe_fabric_recv(tloe_endpoint_t *ep, char *data, size_t len) {
    if (ep->fabric_ops.recv == NULL || ep->fabric_ops.handle == NULL) {
        return -1;
    }
    return ep->fabric_ops.recv(ep->fabric_ops.handle, data, len);
}

int tloe_fabric_open(tloe_endpoint_t *ep, const char *dev_name, const char *dev_opt, int qp) {
    if (ep->fabric_ops.open == NULL)
        return -1;

    ep->fabric_ops.dev_name = (char *)dev_name;
    ep->fabric_ops.dev_opt = (char *)dev_opt;

    ep->fabric_ops.handle = ep->fabric_ops.open((char *)dev_name, (char *)dev_opt, qp);
    return (ep->fabric_ops.handle != NULL) ? 0 : -1;
}

void tloe_fabric_close(tloe_endpoint_t *ep) {
    if (ep->fabric_ops.close == NULL || ep->fabric_ops.handle == NULL)
        return;

    ep->fabric_ops.close(ep->fabric_ops.handle);
    ep->fabric_ops.handle = NULL;
} 
