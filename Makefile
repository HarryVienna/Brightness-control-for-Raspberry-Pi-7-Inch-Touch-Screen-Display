CC = gcc
CFLAGS = -lgpiod -Wall

brightness: brightness.c
	$(CC) -o brightness brightness.c $(CFLAGS)

clean:
	rm -f brightness
