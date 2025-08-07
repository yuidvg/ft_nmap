#pragma once

#include "external.h"

#ifndef IP_ADDRESSES_MAX
#define IP_ADDRESSES_MAX 1024
#endif

#define PORT_UNDEFINED -1
#define PORTS_MAX 1024

/* Macro for initializing fixed-length arrays in immutable structs */
#define INIT_ARRAY_ELEMENT(array, index) .array[index] = array[index]

#define SCAN_TYPE_INDEX_2SCAN_TYPE(index) 1 << index