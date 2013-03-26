CC = gcc
CFLAGS = -Wall

scopegrab:
	$(CC) $(CFLAGS) -o scopegrab scopegrab.c

install:	
	cp scopegrab /usr/local/bin/scopegrab
	cp scopegrab.1 /usr/local/man/man1/scopegrab.1

clean:
	rm -f scopegrab

