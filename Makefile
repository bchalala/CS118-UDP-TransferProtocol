CC=gcc
CFLAGS=-Wall

RECEIVE_SRCS = receiver.c sll.c
RECEIVE_OBJS = $(subst .c,.o,$(RECEIVE_SRCS))

SEND_SRCS = sender.c sll.c
SEND_OBJS = $(subst .c,.o,$(SEND_SRCS))

all: sender receiver

sender: $(SEND_OBJS)
		$(CC) $(CFLAGS) -o $@ $(SEND_OBJS)

receiver: $(RECEIVE_OBJS)
		  $(CC) $(CFLAGS) -o $@ $(RECEIVE_OBJS)

clean:
	rm -rf *.o sender receiver