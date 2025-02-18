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

typedef struct tilelink_header_struct {
    uint8_t reserved1:1;
    uint8_t chan:3;
    uint8_t opcode:3;
    uint8_t reserved2:1;
    uint8_t param:4;
    uint8_t size:4;
    uint8_t domain:8;
    uint16_t reserved;
    uint32_t source:26;
} tl_header_t;

typedef struct {
    tl_header_t header;
    uint64_t address;      // 채널 A, B, C에서 사용
    uint64_t reserved1:38; // 채널 D에서 사용 (Grant)
    uint32_t sink:26;      // 채널 D, E에서 사용
    uint8_t data[];        // 가변 데이터 (AccessAckData, GrantData 등)
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
    uint32_t sink:26;
    // Data
} tl_msgDGrant_t;

typedef struct tilelink_message_e_struct {
    uint8_t reserved1:1;
    uint8_t chan:3;
    uint64_t reserved2:34;
    uint32_t sink:26;
} tl_msgE_t;


// TileLink message with Data and Mask (n <= 6)

// TileLink message with Data and Mask (n >= 6)

// Operations for the TileLink message

#if 0
void tl_set_abc_msg(tl_msg_t *tlmsg, uint8_t chan, uint8_t opcode, uint8_t param, uint8_t size, uint8_t domain, uint32_t source);
uint8_t tl_get_opcode(tl_msg_t *tlmsg);
uint8_t tl_set_opcode(tl_msg_t *tlmsg, uint8_t opcode);
uint8_t tl_get_channel(tl_msg_t *tlmsg);
uint8_t tl_set_channel(tl_msg_t *tlmsg, uint8_t chan);
uint8_t tl_get_credit(tl_msg_t *tlmsg);
#endif

int get_tlmsg_credit(tl_msg_t *tlmsg);

#endif // __TILELINK_MSG_H__
