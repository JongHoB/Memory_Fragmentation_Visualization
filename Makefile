CC = gcc
CFLAGS = -Wall -g

all: main

main: main.c
	$(CC) $(CFLAGS) -o main main.c

clean:
	rm -f main