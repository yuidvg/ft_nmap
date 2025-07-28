# Makefile

CC = gcc
CFLAGS = -Wall
TARGET = pcap_test

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c -lpcap

clean:
	rm -f $(TARGET)
