CC ?= $(CROSS_COMPILE)gcc
CFLAG ?= -g -Wall -Werror
LDFLAG ?= -lpthread -lrt

all: aesdsocket

aesdsocket:
	$(CC) $(CFLAG) -c -o aesdsocket.o aesdsocket.c
	$(CC) $(CFLAG) -o aesdsocket aesdsocket.o $(LDFLAG)

clean:
	rm -f *.o aesdsocket *.elf *.map
