CC = $(CROSS_COMPILE)gcc

all: aesdsocket

aesdsocket:
	$(CC) -g -Wall -c -o aesdsocket.o aesdsocket.c
	$(CC) -g -Wall -o aesdsocket aesdsocket.o

clean:
	rm -f *.o aesdsocket *.elf *.map
