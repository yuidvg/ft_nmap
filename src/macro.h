#pragma once

#include "external.h"

#ifndef IP_ADDRESSES_MAX
#define IP_ADDRESSES_MAX 1024
#endif

#define PORT_UNDEFINED -1
#define PORTS_MAX 1024
#define DEFAULT_PORT 32800
#define MAX_PORT 65535

/* Macro for initializing fixed-length arrays in immutable structs */
#define INIT_ARRAY_ELEMENT(array, index) .array[index] = array[index]

#define SCAN_TYPE_INDEX_2SCAN_TYPE(index) 1 << index
#define OCTET232BITWORD(octet) (octet / 4)

# define DEBUG 0

# ifndef TIMEOUT
#  define TIMEOUT 2
# endif
