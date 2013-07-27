all: example static

static:
	$(CC) -std=c99 -Wall -c *.c
	ar rcs sev.a *.o

example:
	$(MAKE) -C example

clean:
	rm -rf *.a *.o
	$(MAKE) -C example clean

.PHONY: all static example clean
