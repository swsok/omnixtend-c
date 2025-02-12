#ifndef __TLOE_ENDPOINT_H__
#define __TLOE_ENDPOINT_H__

#define MAX_SEQ_NUM     ((1<<10)-1)
#define HALF_MAX_SEQ_NUM ((MAX_SEQ_NUM + 1)/2)

static inline int tloe_seqnum_cmp(int a, int b) {
    int diff = a - b;  // 두 값의 차이 계산

    if (diff == 0) {
        return 0;  // 두 값이 동일
    }
    
    // wrap-around 판단: HALF_MAX_SEQ_NUM 기준으로 앞/뒤 판별
    if (diff > 0) {
        return (diff < HALF_MAX_SEQ_NUM) ? 1 : -1;    // b(100)...a(200);   a(1020)...b(3)
    } else {
        return (-diff < HALF_MAX_SEQ_NUM) ? -1 : 1;
    }
}

static inline int tloe_seqnum_prev(int seq_num) {
	int t = seq_num - 1;
	if (t < 0) 
		t = MAX_SEQ_NUM;
	return t;
}

static inline int tloe_seqnum_next(int seq_num) {
	return (seq_num + 1) & MAX_SEQ_NUM;
}
#endif // __TLOE_ENDPOINT_H__
