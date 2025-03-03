#ifndef __TLOE_ETHER_H__
#define __TLOE_ETHER_H__

#include <stdint.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#define TLOE_ETHER_MAX_QUEUE_NAME_SIZE 256
#define ETHER_PAYLOAD_MAX_LEN 1500
#define ETHER_PAYLOAD_MIN_LEN 46

typedef struct tloe_ether_struct {
    int sock;
    struct sockaddr_ll sll;  //
    uint8_t dest_mac[ETHER_ADDR_LEN];
    uint8_t src_mac[ETHER_ADDR_LEN];
    uint16_t ether_type;
} TloeEther;

typedef struct tloe_ether_ {
    unsigned char dest_mac_addr[6];
    unsigned char src_mac_addr[6];
    unsigned short eth_type;
} tloe_ether_header_t;

size_t tloe_ether_send(TloeEther *, char *, size_t);
size_t tloe_ether_recv(TloeEther *, char *, size_t);
TloeEther *tloe_ether_open(char *, char *, int);
void tloe_ether_close(TloeEther *);

#endif // __TLOE_ETHER_H__
