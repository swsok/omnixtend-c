#ifndef __TLOE_MQ_H__
#define __TLOE_MQ_H__

#include <mqueue.h>

#define TLOE_MQ_PRIORITY 0
#define TLOE_MQ_MSG_SIZE 1600   // >= Ethernet frame (1500)
#define TLOE_MQ_MAX_QUEUE_NAME_SIZE 256

typedef struct tloe_mq_struct {
    mqd_t mq_tx;
    mqd_t mq_rx;
} TloeMQ;

size_t tloe_mq_send(TloeMQ *, char *, size_t);
size_t tloe_mq_recv(TloeMQ *, char *, size_t);
TloeMQ *tloe_mq_open(char *, char *, int);
void tloe_mq_close(TloeMQ *);

#endif // __TLOE_MQ_H__
