#ifndef __TLOE_ETHER_H__
#define __TLOE_ETHER_H__

#include <mqueue.h>

#define TLOE_ETHER_PRIORITY 0
#define TLOE_ETHER_MSG_SIZE 1600   // >= Ethernet frame (1500)
#define TLOE_ETHER_MAX_QUEUE_NAME_SIZE 256

typedef struct tloe_ether_struct {
    mqd_t mq_tx;
    mqd_t mq_rx;
} TloeEther;

typedef struct tloe_ether_ {
    unsigned char dest_mac_addr[6];
    unsigned char src_mac_addr[6];
    unsigned short eth_type;
} tloe_ether_header_t;

size_t tloe_ether_send(TloeEther *eth, char *data, size_t size);
size_t tloe_ether_recv(TloeEther *eth, char *data);
TloeEther *tloe_ether_alloc_and_init(void);
TloeEther *tloe_ether_open(char *queue_name, int master_slave);
void tloe_ether_close(TloeEther *eth);


#endif // __TLOE_ETHER_H__
