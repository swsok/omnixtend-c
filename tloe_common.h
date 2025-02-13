#ifndef __TLOE_COMMON_H__
#define __TLOE_COMMON_H__
#include "tloe_endpoint.h"

#define BUG_ON(condition, message) do { \
    if (condition) { \
        fprintf(stderr, "BUG: %s:%d: %s\n", __FILE__, __LINE__, #message); \
        exit(1); \
    } \
} while (0)

#endif // __TLOE_COMMON_H__

