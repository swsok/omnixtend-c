#ifndef __TILELINK_MSG_H__
#define __TILELINK_MSG_H__

// TileLink messag with Data and without Mask
typedef struct tilelink_message_struct {
	int channel;
	int opcode; 
	int num_flit;

    char header1[8];
    char header2[8];
    char data[0];
} TileLinkMsg;

// TileLink message with Data and Mask (n <= 6)

// TileLink message with Data and Mask (n >= 6)

// Operations for the TileLink message

int get_tlmsg_credit(TileLinkMsg *tlmsg);

#endif // __TILELINK_MSG_H__
