#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "tloe_ether.h"

size_t tloe_ether_send(TloeEther *eth, char *data, size_t size) {
    int ret;

    ret = mq_send(eth->mq_tx, data, size, TLOE_ETHER_PRIORITY);

    return ret;
}

size_t tloe_ether_recv(TloeEther *eth, char *data) {
    int ret;

    ret = mq_receive(eth->mq_rx, data, TLOE_ETHER_MSG_SIZE, NULL);
    if (ret == -1 && errno == EAGAIN) 
		return 0;

    return ret;
}

TloeEther *tloe_ether_alloc_and_init(void){
    TloeEther *ether;

    ether = malloc(sizeof(TloeEther));
    ether->mq_tx = -1;
    ether->mq_rx = -1;

    return ether;
}

TloeEther *tloe_ether_open(char *queue_name, int master_slave) {
    char queue_name_trx[TLOE_ETHER_MAX_QUEUE_NAME_SIZE];
    struct mq_attr attr;
    TloeEther *ether;
    mqd_t mq_tx, mq_rx;

    attr.mq_flags = O_NONBLOCK;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 100;
    attr.mq_msgsize = TLOE_ETHER_MSG_SIZE;
    attr.mq_curmsgs = 0;

    ether = tloe_ether_alloc_and_init();

    // Create message queue
    strcpy(queue_name_trx, queue_name);
    if (master_slave) // if master
        strcat(queue_name_trx, "-a");
    else
        strcat(queue_name_trx, "-b");

    attr.mq_flags = 0;  // blocking mode
    mq_tx = mq_open(queue_name_trx, O_CREAT | O_RDWR, 0644, &attr);
    if (mq_tx == (mqd_t)-1) {
        perror("TX mq_open failed");
        exit(EXIT_FAILURE);
    }

    strcpy(queue_name_trx, queue_name);
    if (master_slave) // if slave
        strcat(queue_name_trx, "-b");
    else
        strcat(queue_name_trx, "-a");

    attr.mq_flags = O_NONBLOCK;
    mq_rx = mq_open(queue_name_trx, O_CREAT | O_RDWR | O_NONBLOCK, 0644, &attr);
    if (mq_rx == (mqd_t)-1) {
        perror("RX mq_open failed");
        exit(EXIT_FAILURE);
    }

    ether->mq_tx = mq_tx;
    ether->mq_rx = mq_rx;

    return ether;
}

void tloe_ether_close(TloeEther *ether) {
    if (ether != NULL && ether->mq_tx != -1)
        mq_close(ether->mq_tx);
    if (ether != NULL && ether->mq_rx != -1)
        mq_close(ether->mq_rx);

    if (ether != NULL)
        free(ether);
}
