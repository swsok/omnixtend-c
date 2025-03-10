#ifndef __TILELINK_MSG_H__
#define __TILELINK_MSG_H__
#include <stdint.h>

#define CHANNEL_NUM     6
#define CHANNEL_0       0
#define CHANNEL_A       1
#define CHANNEL_B       2
#define CHANNEL_C       3
#define CHANNEL_D       4
#define CHANNEL_E       5

#define TL_OPCODE_NUM 	8
// Channel A
#define A_PUTFULLDATA_OPCODE    0
#define A_PUTPARTIALDATA_OPCODE 1
#define A_ARITHMETICDATA_OPCODE 2
#define A_LOGICALDATA_OPCODE    3
#define A_GET_OPCODE            4
#define A_INTENT_OPCODE         5
#define A_ACQUIREBLOCK_OPCODE   6
#define A_ACQUIREPERM_OPCODE    7

// Channel B
#define B_PUTFULLDATA_OPCODE    0
#define B_PUTPARTIALDATA_OPCODE 1
#define B_ARITHMETICDATA_OPCODE 2
#define B_LOGICALDATA_OPCODE    3
#define B_GET_OPCODE            4
#define B_INTENT_OPCODE         5
#define B_PROBEBLOCK_OPCODE     6
#define B_PROBEPERM_OPCODE      7

// Channel C
#define C_ACCESSACK_OPCODE      0
#define C_ACCESSACKDATA_OPCODE  1
#define C_HINTACK_OPCODE        2
#define C_PROBEACK_OPCODE       4
#define C_PROBEACKDATA_OPCODE   5
#define C_RELEASE_OPCODE        6
#define C_RELEASEDATA_OPCODE    7

// Channel D
#define D_ACCESSACK_OPCODE      0
#define D_ACCESSACKDATA_OPCODE  1
#define D_HINTACK_OPCODE        2
#define D_GRANT_OPCODE          4
#define D_GRANTDATA_OPCODE      5
#define D_RELEASEACK_OPCODE     6

// Channel E
#define E_GRANTACK              0  // Not required opcode

typedef struct {
    uint64_t source:26;
    uint64_t reserved:12;
    uint64_t err:2;
    uint64_t domain:8;
    uint64_t size:4;
    uint64_t param:4;
    uint64_t reserved2:1;
    uint64_t opcode:3;
    uint64_t chan:3;
    uint64_t reserved1:1;
} tl_header_t;

typedef struct {
    tl_header_t header;
    uint64_t data[0];
} tl_msg_t;

typedef struct tilelink_message_abc_struct {
    tl_header_t header;
    uint64_t address;
    // Data
} tl_msgABC_t;

typedef struct tilelink_message_d_ack_struct {
    tl_header_t header;
    // Data
} tl_msgDAck_t;

typedef struct tilelink_message_d_grant_struct {
    tl_header_t header;
    uint64_t reserved1:38;
    uint64_t sink:26;
    // Data
} tl_msgDGrant_t;

typedef struct tilelink_message_e_struct {
    uint64_t reserved1:1;
    uint64_t chan:3;
    uint64_t reserved2:34;
    uint64_t sink:26;
} tl_msgE_t;


// TileLink message with Data and Mask (n <= 6)

// TileLink message with Data and Mask (n >= 6)

// Operations for the TileLink message
int tlmsg_get_chan(tl_msg_t *);
int tlmsg_get_opcode(tl_msg_t *);
int tlmsg_get_size(tl_msg_t *);
int tlmsg_get_flits_cnt(tl_msg_t *); 
int tlmsg_get_header_size(tl_msg_t *);
int tlmsg_get_data_size(tl_msg_t *);
int tlmsg_get_total_size(tl_msg_t *);

#endif // __TILELINK_MSG_H__
