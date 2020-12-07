TARGET = /usr/lib/libcrc8.so
LIBS = -lm -lwiringPi
CC = gcc
CFLAGS = -g 

.PHONY: default all clean

default: $(TARGET)
all: default

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

/usr/lib/libcrc8.so:
	sudo $(CC) -c -fPIC crc.c -o crc.o
	sudo $(CC) -shared -Wl,-soname,libcrc8.so -o $@ crc.o

clean:
	sudo rm -f *.o
	sudo rm -f $(TARGET)
