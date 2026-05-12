CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -g -Wall -Werror
LDFLAGS ?= -lpthread -lrt

all: aesdsocket

aesdsocket:
	$(CC) $(CFLAG) -c -o aesdsocket.o aesdsocket.c
	$(CC) $(CFLAGS) -o aesdsocket aesdsocket.o $(LDFLAGS)

clean:
	rm -f *.o aesdsocket *.elf *.map

