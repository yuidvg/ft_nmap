#pragma once

#include "macro.h"

typedef enum e_task_status
{
    DEFAULT,
    CREATED,
    SENT,
    COMPLETED
} t_task_status;

typedef enum e_result
{
    FILTERED,
    OPEN,
    CLOSED,
    UNFILTERED,
    OPEN_FILTERED,
    UNEXPECTED
} t_result;

// Used for the checksum calculation
typedef struct s_pseudo_header
{
    uint32_t src;
    uint32_t dst;
    uint8_t reserved;
    uint8_t protocol;
    uint16_t tcp_length;
} t_pseudo_header;

typedef enum
{
    SCAN_TYPE_SYN = 1 << 0,
    SCAN_TYPE_NULL = 1 << 1,
    SCAN_TYPE_ACK = 1 << 2,
    SCAN_TYPE_FIN = 1 << 3,
    SCAN_TYPE_XMAS = 1 << 4,
    SCAN_TYPE_UDP = 1 << 5,
    SCAN_TYPE_ALL = SCAN_TYPE_SYN | SCAN_TYPE_NULL | SCAN_TYPE_FIN | SCAN_TYPE_XMAS | SCAN_TYPE_ACK | SCAN_TYPE_UDP,
    SCAN_NUMBER = 6
} scan_type_t;

typedef struct
{
    const int ports[PORTS_MAX];
    const char *ip_address;
    const char *const file_path;
    const scan_type_t scan_type;
    const int speedup;
} ft_nmap_args_t;

typedef struct
{
    const int ports[PORTS_MAX];
    const char *ip_addresses[IP_ADDRESSES_MAX];
    const scan_type_t scan_type;
    const int number_of_threads;
} ft_nmap_config_t;

typedef struct
{
    scan_type_t scan_type;
    struct sockaddr_in address;
} probe_t;

typedef struct s_task
{
    probe_t probe;
    t_task_status status;
    t_result result;
    time_t start;
} t_task;

typedef struct
{
    struct ip ip_header;
    struct udphdr udp_header;
    uint8_t payload[48];
} udp_packet_t;

#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif

typedef struct s_thread_args
{
    t_task *tasks;
    int sock;
    pcap_t *handle;
    char iface[IFNAMSIZ]; // might not be necessary to be here
    char local_ip[INET_ADDRSTRLEN];
    uint16_t task_index;
    uint16_t num_tasks;
    bool all_tasks_sent;
    struct timespec start_time;
    struct timespec finish_time;
    scan_type_t scan_type;
} t_thread_args;
