all:
	$(CC) -std=gnu99 -Wall -o client client.c ../*.c -lev
	$(CC) -std=gnu99 -Wall -o server server.c ../*.c -lev

clean:
	rm -rf *.dSYM client server
