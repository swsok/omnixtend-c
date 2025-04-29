TARGETS = tloe_endpoint tloe_ns

CC = gcc
CFLAGS = -g
LDFLAGS = 
SRC_DIR = . util

TEST_NORMAL_FRAME_DROP ?= 0
TEST_TIMEOUT_DROP ?= 0
DEBUG ?= 0
MEMCHECK ?= 0
WDE ?= 0

ifeq ($(TEST_NORMAL_FRAME_DROP),1)
    CFLAGS += -DTEST_NORMAL_FRAME_DROP
endif
ifeq ($(TEST_TIMEOUT_DROP),1)
    CFLAGS += -DTEST_TIMEOUT_DROP
endif
ifeq ($(DEBUG),1)
    CFLAGS += -DDEBUG
endif
ifeq ($(MEMCHECK),1)
    CFLAGS += -fsanitize=address
    LDFLAGS += -fsanitize=address
endif
ifeq ($(WDE),1)
    CFLAGS += -DWDE
endif

SRC_EP = $(filter-out %/tloe_ns.c %/tloe_ns_thread.c, $(wildcard $(addsuffix /*.c, $(SRC_DIR))))
OBJ_EP = $(patsubst %.c, %.o, $(SRC_EP))

SRC_NS = tloe_ns.c tloe_ns_thread.c tloe_mq.c
OBJ_NS = $(patsubst %.c, %.o, $(SRC_NS))

all: $(TARGETS) tags

tloe_endpoint: $(OBJ_EP)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tloe_ns: $(OBJ_NS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

tags:
	ctags -R $(SRC_DIR)

clean:
	rm -f $(OBJ_EP) $(OBJ_NS) $(TARGETS)
	rm -f tags

.PHONY: all clean
