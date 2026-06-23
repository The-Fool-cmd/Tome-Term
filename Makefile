CC      = gcc
CFLAGS  = -Wall -Wextra -g
LDFLAGS = -lncurses -lsqlite3

TARGET  = tome.bin
SRCS    = $(wildcard *.c)
OBJS    = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)
