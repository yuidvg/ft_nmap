# Makefile

CC = gcc
TARGET = pcap_test

all: $(TARGET)

#PCAP_CFLAGS  := $(shell pkg-config --cflags   libpcap)
#PCAP_LDFLAGS := $(shell pkg-config --libs     libpcap)

PCAP_DEFAULT_PATH = /nix/store/cfzm80ii672wf9f24mvncacvjgdriz7v-libpcap-1.10.5/
PCAP_CFLAGS = -I$(PCAP_DEFAULT_PATH)include
PCAP_LDFLAGS = -L$(PCAP_DEFAULT_PATH)lib

CFLAGS  = -Wall $(PCAP_CFLAGS)
LDFLAGS = $(PCAP_LDFLAGS)

$(TARGET): main.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) main.c -lpcap


clean:
	rm -f $(TARGET)
