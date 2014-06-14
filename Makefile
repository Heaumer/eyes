CC = cc # clang
CFLAGS = -ansi -Wall -Wextra -pedantic -g #-O3
TARGET = eyes

all: /usr/include/sys/inotify.h 
	./mkmasks.sh
	$(CC) $(CFLAGS) eyes.c -o $(TARGET)

clean:
	-rm masks.h $(TARGET)


