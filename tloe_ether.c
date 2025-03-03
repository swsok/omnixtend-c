#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "tloe_ether.h"
#include "tloe_common.h"

static void debug_print_tloe_ethhdr(struct ether_header *eh) {
#ifdef DEBUG
    printf("DEST_MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           eh->ether_dhost[0],
           eh->ether_dhost[1],
           eh->ether_dhost[2],
           eh->ether_dhost[3],
           eh->ether_dhost[4],
           eh->ether_dhost[5]);
    printf("SRC_MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           eh->ether_shost[0],
           eh->ether_shost[1],
           eh->ether_shost[2],
           eh->ether_shost[3],
           eh->ether_shost[4],
           eh->ether_shost[5]);
    printf("Ethertpye: %04X\n", ntohs(eh->ether_type));
#endif
}

static void debug_print_actual_send_size(int actual_send_size) {
#ifdef DEBUG
    if (actual_send_size < 0) {
        DEBUG_PRINT("actual_send_size=%d, %s\n", actual_send_size, strerror(errno));
    }
#endif
}

static int tloe_mac_str2bytes(const char *mac_str, uint8_t mac_bytes[6]) {
    if (sscanf(mac_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                &mac_bytes[0], &mac_bytes[1], &mac_bytes[2],
                &mac_bytes[3], &mac_bytes[4], &mac_bytes[5]) != 6) {
        return -1; // Fail
    }
    return 0; // Success
}

size_t tloe_ether_send(TloeEther *eth, char *payload, size_t payload_size) {
    int actual_send_size, send_size;
    unsigned char buffer[ETHER_MAX_LEN];
    struct ether_header *ef = (struct ether_header *) buffer;

    memcpy(ef->ether_dhost, eth->dest_mac, ETHER_ADDR_LEN);
    memcpy(ef->ether_shost, eth->src_mac, ETHER_ADDR_LEN);
    ef->ether_type = eth->ether_type;
    if (payload_size >= ETHERMTU)
        payload_size = ETHERMTU;
    memcpy(buffer+ETHER_HDR_LEN, payload, payload_size);
    //XXX:
    // The minimum padding for an Ethernet frame is 46 bytes; this means that
    // if the actual data payload is less than 46 bytes,
    // padding will be added to reach the minimum Ethernet frame size of 64 bytes.
    //
    // Its minimum size is governed by a requirement for a minimum frame transmission of 64 octets (bytes).
    // With header and FCS taken into account, the minimum payload is 42 octets when an 802.1Q tag is present
    // and 46 octets when absent. When the actual payload is less than the minimum,
    // padding octets are added accordingly.

    send_size = ETHER_HDR_LEN + payload_size;
    if (send_size < ETHER_MIN_LEN)
        send_size = ETHER_MIN_LEN;
    else if (send_size >= ETHER_MAX_LEN)
        send_size = ETHER_MAX_LEN;

retry:
    actual_send_size = sendto(eth->sock, (char *)ef, send_size, 0,
            (struct sockaddr *)&eth->sll, sizeof(eth->sll));
    if (actual_send_size == -1 && errno == EAGAIN) {
        //usleep(10000);
        goto retry;
    }


    debug_print_tloe_ethhdr(ef);
    debug_print_actual_send_size(actual_send_size);

    return actual_send_size;
}

size_t tloe_ether_recv(TloeEther *eth, char *payload, size_t payload_size) {
    int size, copy_size;
    unsigned char buffer[ETHER_MAX_LEN];
    struct ether_header *ef = (struct ether_header *) buffer;

    size = recvfrom(eth->sock, (char *)ef, ETHER_MAX_LEN, 0, NULL, NULL);
    if (size < 0) {
        DEBUG_PRINT("size=%d, %s\n", size, strerror(errno));
        //usleep(500000);
        return size;
    }

    debug_print_tloe_ethhdr(ef);

    if (ef->ether_type != 0xAAAA) {
        copy_size = -1;
        goto out;
    }

    copy_size = size - ETHER_HDR_LEN;
    if (copy_size > payload_size)
        copy_size = payload_size;

    memcpy(payload, (char *) (buffer + ETHER_HDR_LEN), copy_size);

out:
    return copy_size;
}

static TloeEther *tloe_ether_alloc_and_init(uint8_t dest_mac[ETHER_ADDR_LEN], uint8_t src_mac[ETHER_ADDR_LEN]) {
    TloeEther *ether;

    ether = (TloeEther *) malloc(sizeof(TloeEther));
    memset((void *) ether, 0, sizeof(TloeEther));
    ether->sock = -1;
    memset((char *)&ether->sll, 0, sizeof(ether->sll));
    memcpy((char *)&ether->dest_mac, dest_mac, ETHER_ADDR_LEN);
    memcpy((char *)&ether->src_mac, src_mac, ETHER_ADDR_LEN);
    ether->ether_type = htons(0xAAAA);

    return ether;
}

TloeEther *tloe_ether_open(char *ifname, char *dest_mac_str, int create_queue) {
    TloeEther *ether;
    int sockfd;
    struct ifreq ifr;
    struct sockaddr_ll sll, socket_address;
    uint8_t dest_mac[ETHER_ADDR_LEN], src_mac[ETHER_ADDR_LEN];

    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    tloe_mac_str2bytes(dest_mac_str, dest_mac);

    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) < 0) {
        perror("ioctl");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    memcpy(src_mac, (unsigned char *) ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);

    ether = tloe_ether_alloc_and_init(dest_mac, src_mac);

    // Nonblocking mode
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Ethernet interface index
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    DEBUG_PRINT("ifindex: %d\n", ifr.ifr_ifindex);

    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex = if_nametoindex(ifname);
    if (sll.sll_ifindex == 0) {
        perror("if_nametoindex");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (bind(sockfd, (struct sockaddr *)&sll, sizeof(sll)) < 0) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Listening on %s...\n", ifname);

    memset(&socket_address, 0, sizeof(socket_address));
    socket_address.sll_ifindex = ifr.ifr_ifindex;
    socket_address.sll_halen = ETHER_ADDR_LEN;
    memcpy(socket_address.sll_addr, dest_mac, ETHER_ADDR_LEN); // dest MAC

    memcpy(&ether->sll, &socket_address, sizeof(socket_address));

    ether->sock = sockfd;

    return ether;
}

void tloe_ether_close(TloeEther *ether) {
    if (ether != NULL && ether->sock != -1)
        close(ether->sock);

    if (ether != NULL)
        free(ether);
}

static void set_tloe_ether(tloe_ether_header_t *tloeether, char *dest, char *src, unsigned short eth_type) {
    memcpy(tloeether->dest_mac_addr, dest, 6);
    memcpy(tloeether->src_mac_addr, src, 6);
    tloeether->eth_type = eth_type;
}

static tloe_ether_header_t get_tloe_ether(void) {
    tloe_ether_header_t tloeether;

    unsigned char dest_mac[6] = {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA};
    unsigned char src_mac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    unsigned short eth_type = 0xAAAA;

    set_tloe_ether(&tloeether, (char *)dest_mac, (char *)src_mac, eth_type);

    return tloeether;
}
