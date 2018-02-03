CC=gcc 
CFLAGS=-Wall -w

all: test
program: test.o
program.o: test.c

clean:
	rm -f test.o
run: test
	./program

all: $(OBJS) clean
