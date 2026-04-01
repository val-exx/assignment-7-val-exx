CC = $(CROSS_COMPILE)gcc

all: writer

writer:
	$(CC) -g -Wall -c -o writer.o writer.c
	$(CC) -g -Wall -o writer.o writer

clean:
	rm -f *.o writer *.elf *.map
