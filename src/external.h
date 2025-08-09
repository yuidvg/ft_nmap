#pragma once

// BSD-style type definitions for libpcap compatibility
#ifndef _U_CHAR_DEFINED
#define _U_CHAR_DEFINED
typedef unsigned char u_char;
#endif

#ifndef _U_SHORT_DEFINED
#define _U_SHORT_DEFINED
typedef unsigned short u_short;
#endif

#ifndef _U_INT_DEFINED
#define _U_INT_DEFINED
typedef unsigned int u_int;
#endif

#if defined(__linux__)
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif
#if defined(__APPLE__)
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif
#endif
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <pcap.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
