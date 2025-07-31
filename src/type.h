#pragma once

#include "external.h"
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
    const int ports[1024];
    const char *const ip_address;
    const char *const file_path;
    const scan_type_t scan_type;
    const int speedup;
} ft_nmap_args_t;

char * filepath2ip_addresses(const char *const filepath);

typedef struct
{
    const int ports[1024];
    const char *const ip_addresses[IP_ADDRESSES_MAX];
    const scan_type_t scan_type;
    const int number_of_threads;
} ft_nmap_config_t;



