all: static-lib example

static-lib: sev.c sev.h sev_queue.c sev_queue.h
	$(CC) -Wall -c sev.c
	$(CC) -Wall -c sev_queue.c
	libtool -static -o sev.a sev.o sev_queue.o

example: static-lib example.c
	$(CC) -Wall -o example example.c sev.a -lev

clean:
	rm -rf *.dSYM *.a *.o example
