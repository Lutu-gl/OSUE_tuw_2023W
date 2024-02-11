# Makefile for exercise 3
# author: Luca (xxxxxxxx) <exxxxxxxx@student.tuwien.ac.at>
# date: 2023-12-26

CC = gcc
DEFS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS = -Wall -std=c99 -pedantic -g $(DEFS) 
LDFLAGS = -lrt -pthread -lpthread -lm -lz

OBJECTS1 = client.o connection.o
OBJECTS2 = server.o connection.o

.PHONY: all clean
all: client server

client: $(OBJECTS1)
	@echo "Linking and producting the client"
	$(CC) -o $@ $^ $(LDFLAGS)

server: $(OBJECTS2)
	@echo "Linking and producting the server"
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

client.o: client.c connection.h
server.o: server.c connection.h
connection.o: connection.c connection.h

clean:
	@echo "Removing everything but the source files"
	rm -f client.o client server.o server connection.o
