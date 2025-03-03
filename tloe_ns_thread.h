#ifndef TLOE_NS_THREAD_H
#define TLOE_NS_THREAD_H

void tloe_ns_thread_start(const char *);
void tloe_ns_thread_stop(void);
void tloe_ns_flush_ports(void);
int tloe_ns_thread_is_done(void);

#endif