# Makefile

CC = gcc
CFLAGS = -Wall -I /nix/store/cfzm80ii672wf9f24mvncacvjgdriz7v-libpcap-1.10.5/include
TARGET = pcap_test

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -L/nix/store/cfzm80ii672wf9f24mvncacvjgdriz7v-libpcap-1.10.5/lib -o $(TARGET) main.c -lpcap

clean:
	rm -f $(TARGET)
