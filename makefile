all: static-lib example

static-lib: sev.c sev.h
	$(CC) -Wall -c sev.c
	libtool -static -o sev.a sev.o

example: static-lib example.c
	$(CC) -Wall -o example example.c sev.a -lev

clean:
	rm -rf *.dSYM *.a *.o example
