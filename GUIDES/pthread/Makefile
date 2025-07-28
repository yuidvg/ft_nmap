# Makefile

CC = gcc
CFLAGS = -Wall -pthread
TARGET = thread_test

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c -lpcap -I/nix/store/cfzm80ii672wf9f24mvncacvjgdriz7v-libpcap-1.10.5/include -L/nix/store/cfzm80ii672wf9f24mvncacvjgdriz7v-libpcap-1.10.5/lib

clean:
	rm -f $(TARGET)
