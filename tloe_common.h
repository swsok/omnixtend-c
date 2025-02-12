#ifndef __TLOE_COMMON_H__
#define __TLOE_COMMON_H__
#include "tloe_endpoint.h"

static inline int seq_num_diff(int seq1, int seq2) {
	int diff = seq2 - seq1;
	if (diff < 0)
		diff += (MAX_SEQ_NUM + 1); 

	return diff;
}

#define BUG_ON(condition, message) do { \
    if (condition) { \
        fprintf(stderr, "BUG: %s:%d: %s\n", __FILE__, __LINE__, #message); \
        exit(1); \
    } \
} while (0)

#endif // __TLOE_COMMON_H__

