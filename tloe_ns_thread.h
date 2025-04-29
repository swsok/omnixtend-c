#ifndef TLOE_NS_THREAD_H
#define TLOE_NS_THREAD_H

void tloe_ns_thread_start(const char *);
void tloe_ns_thread_stop(void);
void tloe_ns_flush_ports(void);
int tloe_ns_thread_is_done(void);

// Drop request control functions
void tloe_ns_set_drop_count_a_to_b(int);
void tloe_ns_set_drop_count_b_to_a(int);
void tloe_ns_set_drop_count_bidirectional(int);
#endif