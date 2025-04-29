#ifndef TLOE_NSM_H
#define TLOE_NSM_H

#include "tloe_endpoint.h"
#include "tloe_frame.h"

#ifdef TEST_TIMEOUT_DROP
int handle_ack_packet_drop(tloe_endpoint_t *, tloe_frame_t *);
int handle_normal_frame_drop(tloe_endpoint_t *);
int handle_single_normal_frame_drop(tloe_endpoint_t *e);
#else
static inline int handle_ack_packet_drop(tloe_endpoint_t *e, tloe_frame_t *frame) {
    return 0;
}

static inline int handle_normal_frame_drop(tloe_endpoint_t *e) {
    return 0;
}

static inline int handle_single_normal_frame_drop(tloe_endpoint_t *e) {
    return 0;
}
#endif


#endif // TLOE_NSM_H 