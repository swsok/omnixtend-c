#ifndef __TLOE_COMMON_H__
#define __TLOE_COMMON_H__
#include "tloe_endpoint.h"

#define BUG_ON(condition, message) do { \
    if (condition) { \
        fprintf(stderr, "BUG: %s:%d: %s\n", __FILE__, __LINE__, #message); \
        exit(1); \
    } \
} while (0)

#ifdef DEBUG
#define DEBUG_PRINT(fmt, ...) \
        fprintf(stderr, "[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...) (void)0
#endif

#endif // __TLOE_COMMON_H__

