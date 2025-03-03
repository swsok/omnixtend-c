#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <mqueue.h>

#include "tloe_mq.h"
#include "tloe_fabric.h"

size_t tloe_mq_send(TloeMQ *mq, char *data, size_t size) {
    int ret;

    ret = mq_send(mq->mq_tx, data, size, TLOE_MQ_PRIORITY);

    return ret;
}

size_t tloe_mq_recv(TloeMQ *eth, char *data, size_t size) {
    int ret;

    ret = mq_receive(eth->mq_rx, data, TLOE_MQ_MSG_SIZE, NULL);

    return ret;
}

static TloeMQ *tloe_mq_alloc_and_init(void){
    TloeMQ *ether;

    ether = malloc(sizeof(TloeMQ));
    ether->mq_tx = -1;
    ether->mq_rx = -1;

    return ether;
}

static mqd_t create_message_queue(const char *queue_name, const char *suffix,
                                  struct mq_attr *attr, int flags)
{
    char queue_name_trx[TLOE_MQ_MAX_QUEUE_NAME_SIZE];
    mqd_t mq;

    strcpy(queue_name_trx, queue_name);
    strcat(queue_name_trx, suffix);

    mq = mq_open(queue_name_trx, O_RDWR | flags, 0644, attr);
    if (mq == (mqd_t)-1) {
        fprintf(stderr, "mq_open failed. errno: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    return mq;
}

TloeMQ *tloe_mq_open(char *queue_name, char *master, int qflags) {
    struct mq_attr attr;
    TloeMQ *ether;
    mqd_t mq_tx, mq_rx;
    int mq_oflags;
    const char *tx_suffix, *rx_suffix;

    attr.mq_flags = 0;
    attr.mq_maxmsg = 100;
    attr.mq_msgsize = TLOE_MQ_MSG_SIZE;
    attr.mq_curmsgs = 0;

    ether = tloe_mq_alloc_and_init();

    // Set suffixes based on master/slave role
    if (strncmp(master, "-master", sizeof("-master")) == 0) {
        tx_suffix = "-a";
        rx_suffix = "-b";
    } else {
        tx_suffix = "-b";
        rx_suffix = "-a";
    }

    // Create TX queue (blocking mode)
    mq_oflags = ((qflags & TLOE_FABRIC_QFLAGS_CREATE) ? O_CREAT : 0) | O_NONBLOCK;
    attr.mq_flags = O_NONBLOCK;
    mq_tx = create_message_queue(queue_name, tx_suffix, &attr, mq_oflags);

    // Create RX queue (non-blocking mode)
    mq_oflags = ((qflags & TLOE_FABRIC_QFLAGS_CREATE) ? O_CREAT : 0) | O_NONBLOCK;
    attr.mq_flags = O_NONBLOCK;
    mq_rx = create_message_queue(queue_name, rx_suffix, &attr, mq_oflags);

    ether->mq_tx = mq_tx;
    ether->mq_rx = mq_rx;

    return ether;
}

void tloe_mq_close(TloeMQ *ether) {
    if (ether != NULL && ether->mq_tx != -1)
        mq_close(ether->mq_tx);
    if (ether != NULL && ether->mq_rx != -1)
        mq_close(ether->mq_rx);

    if (ether != NULL)
        free(ether);
}
