CC = $(CROSS_COMPILE)gcc

all: writer

writer:
	$(CC) -g -Wall -c -o writer.o writer.c
	$(CC) -g -Wall -o writer writer.o

clean:
	rm -f *.o writer *.elf *.map
