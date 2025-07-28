# Makefile

CC = gcc
CFLAGS = -Wall -pthread
TARGET = thread_test

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

clean:
	rm -f $(TARGET)
