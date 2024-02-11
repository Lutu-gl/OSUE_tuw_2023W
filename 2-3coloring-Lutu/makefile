# Makefile for exercise 2: 3 color algorithm
# author: Luca (xxxxxxx) <exxxxxxx@student.tuwien.ac.at>
# date: 2023-12-07

CC = gcc
DEFS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS = -Wall -std=c99 -pedantic -g $(DEFS) 
LDFLAGS = -lrt -pthread -lpthread -lm

OBJECTS1 = generator.o cbuffer.o
OBJECTS2 = supervisor.o cbuffer.o

.PHONY: all clean
all: generator supervisor

generator: $(OBJECTS1)
	@echo "Linking and producting the generator"
	$(CC) -o $@ $^ $(LDFLAGS)

supervisor: $(OBJECTS2)
	@echo "Linking and producting the supervisor"
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

generator.o: generator.c cbuffer.h
supervisor.o: supervisor.c cbuffer.h
cbuffer.o: cbuffer.c cbuffer.h

clean:
	@echo "Removing everything but the source files"
	rm -f generator.o generator cbuffer.o supervisor.o supervisor
