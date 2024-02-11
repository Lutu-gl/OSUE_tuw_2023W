# Makefile for program forkFFT.c
# author: Luca (xxxxxxx) <exxxxxxx@student.tuwien.ac.at>
CFLAGS = -std=c99 -pedantic -Wall -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L -g -lm

all: final

final: forkFFT.o
	@echo "Linking and producting the final app"
	gcc $(CFLAGS) forkFFT.o -o forkFFT

forkFFT.o: forkFFT.c
	@echo "Compiling file"
	gcc $(CFLAGS) -c forkFFT.c

clean:
	@echo "Removing everything but the source files"
	rm -f forkFFT.o forkFFT
