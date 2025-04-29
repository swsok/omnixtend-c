#include "tloe_common.h"
#include "tloe_endpoint.h"
#include "tloe_frame.h"

#ifdef TEST_TIMEOUT_DROP
int handle_ack_packet_drop(tloe_endpoint_t *e, tloe_frame_t *ack_frame) {
    int should_drop = 0;

    if (e->master) 
	goto out;

    if (e->drop_apacket_size == 0 && ((rand() % 1000) == 0)) {
        e->drop_apacket_size = 4;
    }
    if (e->drop_apacket_size > 0) {
        e->drop_apacket_cnt++;
        e->drop_apacket_size--;
        should_drop = 1;
    }
out:
    return should_drop;
}

int handle_normal_frame_drop(tloe_endpoint_t *e) {
    int should_drop = 0;

    if (e->master)
        goto out;

    if (e->drop_npacket_size == 0 && ((rand() % 1000) == 0))
        e->drop_npacket_size = 4;
    if (e->drop_npacket_size > 0) {
        e->drop_npacket_cnt++;
        e->drop_npacket_size--;
        should_drop = 1;
    }
out:
    return should_drop;
}
#endif

#ifdef TEST_NORMAL_FRAME_DROP
int handle_single_normal_frame_drop(tloe_endpoint_t *e) {
    if ((rand() % 10000) != 99)
        return 0;

    e->drop_cnt++;
    return 1;
}
#endif
