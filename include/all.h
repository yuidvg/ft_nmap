#pragma once

// Standard libc
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <errno.h>
# include <time.h>
// Posix stuff, not libc but pretty necessary. Still fits the spirit of the rule since it's portable
# include <netinet/ip.h>
# include <netinet/ip_icmp.h>
# include <netinet/tcp.h>
# include <netinet/udp.h>
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
# include <pthread.h>
# include <../libft/libft.h>

# define DEBUG 0
# define DEFAULT_PORT 32800
# ifndef TIMEOUT
#  define TIMEOUT 2
# endif

typedef enum { false, true } bool;

typedef enum    e_task_status {
    DEFAULT,
    CREATED,
    SENT,
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
    CLOSED,
    UNFILTERED,
    OPEN_FILTERED,
    UNEXPECTED
}               t_result;

typedef struct  s_probe {
    t_scan_types   scan_type;   // Type of scan to perform
    struct sockaddr_in addr;    // Address to scan
}               t_probe;

typedef struct  s_task {
    t_probe         probe;
    t_task_status   status;
    t_result        result;
    time_t          start;
}               t_task;

typedef struct  s_thread_args {
    t_task          *tasks;
    int             sock;
    pcap_t          *handle;
    char            iface[IFNAMSIZ];            //might not be necessary to be here
    char            local_ip[INET_ADDRSTRLEN];;
    uint16_t        task_index;
    uint16_t        num_tasks;
    bool            all_tasks_sent;
    time_t          finish_time;
}               t_thread_args;

// Used for the checksum calculation
typedef struct s_pseudo_header {
    uint32_t src;
    uint32_t dst;
    uint8_t reserved;
    uint8_t protocol;
    uint16_t tcp_length;
}               t_pseudo_header;

//print.c
void print_results(t_task *tasks, const uint16_t num_tasks);

//receive.c
void handle_packet(t_task *tasks, const u_char *data, const uint16_t num_tasks);
bool check_for_answer(t_task *tasks, pcap_t *handle, const uint16_t num_tasks);

//send.c
unsigned short checksum(unsigned short *ptr, int nbytes);
void send_packet(const char *local_ip, const int sock, const t_probe p);

//setup.c
int get_local_ip_and_iface(char *iface, char *ip);
int setup_socket();
pcap_t *setup_handle(const char *iface);
void setup_filter(pcap_t *handle, char *local_ip);

//main.c
t_task *build_tasks(const t_probe *probes, const uint16_t num_probes);
void *worker(void *arg);

//TEMPORARY
t_probe *build_probes(const uint16_t num_probes);

/*
////TODO
    Improve the way we handle timeouts (currently we're waiting until the last possible time, despite the fast that the last signal sent may have been answered already)
    Improve setup_filter to be more specific
    Improve error handling (errbuf, malloc returns, etc.)
*/

typedef struct
{
    struct ip ip_header;
    struct udphdr udp_header;
    uint8_t payload[24];
} udp_packet_t;
