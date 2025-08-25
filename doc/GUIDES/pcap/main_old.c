#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pcap.h>
#include <sys/time.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <net/ethernet.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define TIMEOUT_MS 5000


char *get_first_device() {
    pcap_if_t *alldevs;
    char errbuf[PCAP_ERRBUF_SIZE];
    if (pcap_findalldevs(&alldevs, errbuf) == -1) {
        fprintf(stderr, "pcap_findalldevs error: %s\n", errbuf);
        return NULL;
    }
    if (!alldevs) {
        fprintf(stderr, "No devices found\n");
        return NULL;
    }
    // Return the name of the first device (caller must NOT free)
    char *devname = alldevs->name;

    // Note: do NOT free alldevs here if you plan to use devname, or copy devname
    // Instead, free the list but keep a copy of the string:
    static char first_dev[256];
    snprintf(first_dev, sizeof(first_dev), "%s", devname);

    pcap_freealldevs(alldevs);
    return first_dev;
}

int get_local_ip(const char *target_ip, char *local_ip, size_t len) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);  // UDP socket (no actual data sent)
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in remote_addr;
    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(443);  // Arbitrary port
    if (inet_pton(AF_INET, target_ip, &remote_addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid target IP: %s\n", target_ip);
        close(sock);
        return -1;
    }
    if (connect(sock, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    struct sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);
    if (getsockname(sock, (struct sockaddr *)&local_addr, &addr_len) < 0) {
        perror("getsockname");
        close(sock);
        return -1;
    }

    const char *p = inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, len);
    close(sock);

    return (p != NULL) ? 0 : -1;
}

// Checksum functions
unsigned short csum(unsigned short *ptr, int nbytes) {
    long sum = 0;
    unsigned short oddbyte;
    short answer;

    while (nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }

    if (nbytes == 1) {
        oddbyte = 0;
        *((unsigned char *) &oddbyte) = *(unsigned char *) ptr;
        sum += oddbyte;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = (short) ~sum;

    return answer;
}

unsigned short tcp_checksum(struct iphdr *iph, struct tcphdr *tcph) {
    struct pseudo_header {
        unsigned int src_addr;
        unsigned int dst_addr;
        unsigned char zero;
        unsigned char protocol;
        unsigned short tcp_length;
    };

    char buf[4096];
    struct pseudo_header psh;

    int tcp_len = ntohs(iph->tot_len) - iph->ihl * 4;

    psh.src_addr = iph->saddr;
    psh.dst_addr = iph->daddr;
    psh.zero = 0;
    psh.protocol = IPPROTO_TCP;
    psh.tcp_length = htons(tcp_len);

    memcpy(buf, &psh, sizeof(psh));
    memcpy(buf + sizeof(psh), tcph, tcp_len);

    return csum((unsigned short *) buf, tcp_len + sizeof(psh));
}

int test_port(const char *target_ip, int port, int timeout_ms) {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle;
    char filter_exp[128];
    struct bpf_program fp;
    bpf_u_int32 net = 0;

    /*
    char *device = pcap_lookupdev(errbuf);
    if (!device) {
        fprintf(stderr, "Failed to find default device: %s\n", errbuf);
        return -1;
    }
    printf("Listening on device: %s\n", device);
    */
    char *device = get_first_device();
    printf("Using device: %s\n", device);
    ///const char *device = "any";  // or use pcap_lookupdev(NULL)
    //handle = pcap_open_live(device, BUFSIZ, 1, timeout_ms, errbuf);  // 1 = promiscuous
    handle = pcap_open_live(device, BUFSIZ, 1, timeout_ms, errbuf);
    if (!handle) {
        fprintf(stderr, "pcap_open_live failed: %s\n", errbuf);
        return -1;
    }

    // Create raw socket to send
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    int one = 1;
    setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));

    // Create IP/TCP packet
    char packet[4096];
    memset(packet, 0, sizeof(packet));

    struct iphdr *iph = (struct iphdr *) packet;
    struct tcphdr *tcph = (struct tcphdr *) (packet + sizeof(struct iphdr));

    iph->ihl = 5;
    iph->version = 4;
    iph->tos = 0;
    iph->tot_len = htons(sizeof(struct iphdr) + sizeof(struct tcphdr));
    iph->id = htons(rand() % 65535);
    iph->frag_off = 0;
    iph->ttl = 64;
    iph->protocol = IPPROTO_TCP;
    char local_ip[INET_ADDRSTRLEN];
    if (get_local_ip(target_ip, local_ip, sizeof(local_ip)) != 0) {
        fprintf(stderr, "Failed to detect local IP\n");
        return -1;
    }
    iph->saddr = inet_addr(local_ip);
    printf("Source IP (iph->saddr): %s\n", inet_ntoa(*(struct in_addr *)&iph->saddr));
    iph->daddr = inet_addr(target_ip);

    int src_port = 32768 + rand() % (65535 - 32768);
    tcph->source = htons(src_port);
    //tcph->source = htons(12345);
    tcph->dest = htons(port);
    tcph->seq = htonl(rand());
    tcph->doff = 5;
    tcph->syn = 1;
    tcph->window = htons(5840);

    tcph->check = tcp_checksum(iph, tcph);
    iph->check = csum((unsigned short *) packet, iph->ihl * 4);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = iph->daddr;

    // Setup BPF filter
    snprintf(filter_exp, sizeof(filter_exp),
         "tcp and src host %s and dst port %d", target_ip, src_port);

    if (pcap_compile(handle, &fp, filter_exp, 0, net) == -1) {
        fprintf(stderr, "pcap_compile error: %s\n", pcap_geterr(handle));
        return -1;
    }

    if (pcap_setfilter(handle, &fp) == -1) {
        fprintf(stderr, "pcap_setfilter error: %s\n", pcap_geterr(handle));
        return -1;
    }

    // Send SYN packet
    if (sendto(sock, packet, sizeof(struct iphdr) + sizeof(struct tcphdr), 0,
               (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        perror("sendto");
        return -1;
    }

    // Wait for response
    struct pcap_pkthdr *header;
    const u_char *data;
    int res;

    while ((res = pcap_next_ex(handle, &header, &data)) >= 0) {
        if (res == 0) continue;  // Timeout

        int linktype = pcap_datalink(handle);
        int ip_offset = 0;

        switch (linktype) {
            case DLT_EN10MB:
                ip_offset = 14;  // Ethernet header
                break;
            case DLT_NULL:
            case DLT_LOOP:   // same treatment
                ip_offset = 4;  // Loopback/Null header size
                break;
            case DLT_RAW:
                ip_offset = 0;  // Raw IP, no link layer header
                break;
            default:
                fprintf(stderr, "Unsupported datalink: %d\n", linktype);
                return -1;
        }

        struct iphdr *resp_iph = (struct iphdr *) (data + ip_offset);
        //struct iphdr *resp_iph = (struct iphdr *) (data + 14); // Skip Ethernet
        struct tcphdr *resp_tcph = (struct tcphdr *) ((u_char *) resp_iph + resp_iph->ihl * 4);

        if (resp_tcph->source == htons(port) && resp_tcph->dest == htons(12345)) {
            if (resp_tcph->syn && resp_tcph->ack) {
                printf("Port %d is OPEN (SYN-ACK received)\n", port);
                close(sock);
                pcap_close(handle);
                return 1;
            } else if (resp_tcph->rst) {
                printf("Port %d is CLOSED (RST received)\n", port);
                close(sock);
                pcap_close(handle);
                return 0;
            }
        }
    }

    printf("Port %d is FILTERED (no response)\n", port);
    close(sock);
    pcap_close(handle);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <target_ip> <port>\n", argv[0]);
        printf("Example: %s 8.8.8.8 80\n", argv[0]);
        return 1;
    }

    const char *target_ip = argv[1];
    int port = atoi(argv[2]);

    if (port <= 0 || port > 65535) {
        printf("Invalid port number: %d. Must be between 1 and 65535.\n", port);
        return 1;
    }

    printf("Performing SYN scan on %s:%d...\n", target_ip, port);
    test_port(target_ip, port, TIMEOUT_MS);

    return 0;
}
