# Makefile

CC = gcc
TARGET = pcap_test

all: $(TARGET)

PCAP_CFLAGS  := $(shell pkg-config --cflags   libpcap)
PCAP_LDFLAGS := $(shell pkg-config --libs     libpcap)

CFLAGS  = -Wall $(PCAP_CFLAGS)
LDFLAGS = $(PCAP_LDFLAGS)

$(TARGET): main.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) main.c

clean:
	rm -f $(TARGET)
