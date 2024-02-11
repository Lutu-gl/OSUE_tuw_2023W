# Makefile for program: mygrep.c
# Author: Luca, xxxxxxxx <exxxxxxxx@student.tuwien.ac.at>
CFLAGS = -std=c99 -pedantic -Wall -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L -g

all: final

final: mygrep.o
	@echo "Linking and producing the final app"
	gcc $(CFLAGS) mygrep.o -o mygrep

mygrep.o: mygrep.c
	@echo "Compiling file"
	gcc $(CFLAGS) -c mygrep.c

clean:
	@echo "Removing everything but the source files"
	rm -f mygrep mygrep.o
