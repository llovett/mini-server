TARGETS=hw2
CC=gcc
CFLAGS=-O2 -lpthread

hw2: hw2.c
	$(CC) $(CFLAGS) -o hw2 hw2.c

all: $(TARGETS)

clean:
	rm -f $(TARGETS)
