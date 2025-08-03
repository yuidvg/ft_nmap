#ifndef FT_NMAP_H
# define FT_NMAP_H

// Standard libc
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <errno.h>
# include <time.h>
// Posix stuff, not libc but pretty necessary. Still fits the spirit of the rule since it's portable
# include <netinet/ip.h>
# include <netinet/tcp.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <sys/socket.h>
# include <sys/ioctl.h>
# include <sys/select.h>
# include <net/if.h>
# include <netdb.h>
# include <ifaddrs.h>
// External libraries
# include <pcap.h>

//# define PACKET_LEN 4096

typedef enum { false, true } bool;

typedef enum    e_task_status {
    CREATED,
    FUNCTION_RAN,
    COMPLETED
}               t_task_status;

typedef enum    e_scan_types {
    SYN,
    NUL,
    ACK,
    FIN,
    XMAS,
    UDP,
    scan_types_len
}               t_scan_types;

typedef enum    e_result {
    FILTERED,
    OPEN,
    CLOSED
}               t_result;

typedef struct  s_probe {
    t_scan_types   scan_type; // Type of scan to perform
    struct sockaddr_in addr;   // Address to scan
}               t_probe;

typedef struct  s_task {
    t_probe         probe;
    t_task_status   status;
    t_result        result;
    time_t          start;
}               t_task;

// IP and TCP headers
struct s_pseudo_header {
    uint32_t src;
    uint32_t dst;
    uint8_t reserved;
    uint8_t protocol;
    uint16_t tcp_length;
};

#endif