#pragma once

#include "macro.h"

typedef enum
{
    SCAN_TYPE_SYN = 1 << 0,
    SCAN_TYPE_NULL = 1 << 1,
    SCAN_TYPE_FIN = 1 << 2,
    SCAN_TYPE_XMAS = 1 << 3,
    SCAN_TYPE_ACK = 1 << 4,
    SCAN_TYPE_UDP = 1 << 5,
    SCAN_TYPE_ALL = SCAN_TYPE_SYN | SCAN_TYPE_NULL | SCAN_TYPE_FIN | SCAN_TYPE_XMAS | SCAN_TYPE_ACK | SCAN_TYPE_UDP,
} scan_type_t;

typedef struct
{
    const bool help;
    int ports[PORTS_MAX];
    const char * ip_addresses[IP_ADDRESSES_MAX];
    const char *const file_path;
    const scan_type_t scan_type;
    const int speedup;
} ft_nmap_args_t;

typedef struct
{
    const int ports[PORTS_MAX];
    const char * ip_addresses[IP_ADDRESSES_MAX];
    const scan_type_t scan_type;
    const int number_of_threads;
} ft_nmap_config_t;

typedef struct
{
    const char *const host;
    const int port;
    const scan_type_t scan_type;
} probe_t;

// Memory management types
typedef struct memory_block
{
    void *const ptr;
    const size_t size;
    struct memory_block *const next;
} memory_block_t;

typedef struct
{
    memory_block_t *head;
    pthread_mutex_t mutex;
} memory_manager_t;
