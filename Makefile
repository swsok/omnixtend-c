CC = gcc
CFLAGS = -g
SRC_DIR = . util

TEST_NORMAL_FRAME_DROP ?= 0

ifeq ($(TEST_NORMAL_FRAME_DROP),1)
    CFLAGS += -DTEST_NORMAL_FRAME_DROP
endif

SRC = $(wildcard $(addsuffix /*.c, $(SRC_DIR)))
OBJ = $(patsubst %.c, %.o, $(SRC))
TARGET = tloe_endpoint

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean

