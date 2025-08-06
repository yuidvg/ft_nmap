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
# include <pthread.h>

# define TIMEOUT 2

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
    UNEXPECTED
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
    void            (*function)(const char*, int, struct sockaddr_in, int);
}               t_task;

typedef struct  s_thread_args {
    t_task          *tasks;
    int             sock;
    pcap_t          *handle;
    char            iface[IFNAMSIZ];            //might not be necessary to be here
    char            local_ip[INET_ADDRSTRLEN];;
    int             task_index;
    uint16_t        num_tasks;
}               t_thread_args;

// Used for the checksum calculation
typedef struct s_pseudo_header {
    uint32_t src;
    uint32_t dst;
    uint8_t reserved;
    uint8_t protocol;
    uint16_t tcp_length;
}               t_pseudo_header;

#endif

/*
//CORE
    main
    worker
    build_tasks

//SETUP SCAN
    setup_socket
    get_local_ip_and_iface
    setup_handle
    setup_filter

//SENDING
checksum
send_syn_packet

//RECEIVING
check_for_answer
handle_packet

//PRINTS
    print_results

//tmp
build_probes
*/

/*
////TODO
    Improve the way we handle timeouts (currently we're waiting until the last possible time, despite the fast that the last signal sent may have been answered already)
    Improve setup_filter to be more specific
    Improve error handling (errbuf, malloc returns, etc.)
    change types to be more specific (e.g. use uint16_t, ...)
*/