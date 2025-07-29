#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pcap.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <errno.h>
#include <time.h>

#define PACKET_LEN 4096

// IP and TCP headers
struct pseudo_header {
    u_int32_t src;
    u_int32_t dst;
    u_int8_t reserved;
    u_int8_t protocol;
    u_int16_t tcp_length;
};

// Checksum function
unsigned short checksum(unsigned short *ptr, int nbytes) {
    long sum = 0;
    unsigned short oddbyte;
    while (nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }
    if (nbytes == 1) {
        oddbyte = 0;
        *((unsigned char*)&oddbyte) = *(unsigned char*)ptr;
        sum += oddbyte;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

// Get the default interface and IP
int get_local_ip_and_iface(char *iface, char *ip) {
    struct ifaddrs *ifaddr, *ifa;
    int family;
    if (getifaddrs(&ifaddr) == -1) return -1;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        family = ifa->ifa_addr->sa_family;
        if ((family == AF_INET) && !(ifa->ifa_flags & IFF_LOOPBACK)) {
            strcpy(iface, ifa->ifa_name);
            getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), ip, INET_ADDRSTRLEN, NULL, 0, NI_NUMERICHOST);
            freeifaddrs(ifaddr);
            return 0;
        }
    }
    freeifaddrs(ifaddr);
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <target_ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *target_ip = argv[1];
    int port = atoi(argv[2]);

    char iface[IFNAMSIZ], local_ip[INET_ADDRSTRLEN];
    if (get_local_ip_and_iface(iface, local_ip) != 0) {
        perror("Could not determine local IP/interface");
        exit(EXIT_FAILURE);
    }
    printf("[*] Using interface: %s (%s)\n", iface, local_ip);

    // Raw socket
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set IP_HDRINCL
    int one = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("Setting IP_HDRINCL");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Packet buffer
    char packet[PACKET_LEN];
    memset(packet, 0, PACKET_LEN);

    struct ip *iph = (struct ip *) packet;
    struct tcphdr *tcph = (struct tcphdr *) (packet + sizeof(struct ip));

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    inet_pton(AF_INET, target_ip, &dest.sin_addr);

    // Fill IP header
    iph->ip_hl = 5;
    iph->ip_v = 4;
    iph->ip_tos = 0;
    iph->ip_len = htons(sizeof(struct ip) + sizeof(struct tcphdr));
    iph->ip_id = htons(rand() % 65535);
    iph->ip_off = 0;
    iph->ip_ttl = 64;
    iph->ip_p = IPPROTO_TCP;
    iph->ip_sum = 0;
    iph->ip_src.s_addr = inet_addr(local_ip);
    iph->ip_dst = dest.sin_addr;

    // Fill TCP header
    tcph->th_sport = htons(32768 + rand() % (65535 - 32768));
    tcph->th_dport = htons(port);
    tcph->th_seq = htonl(0);
    tcph->th_ack = 0;
    tcph->th_off = 5;
    tcph->th_flags = TH_SYN;
    tcph->th_win = htons(1024);
    tcph->th_sum = 0;
    tcph->th_urp = 0;

    // Calculate TCP checksum
    struct pseudo_header psh;
    psh.src = iph->ip_src.s_addr;
    psh.dst = iph->ip_dst.s_addr;
    psh.reserved = 0;
    psh.protocol = IPPROTO_TCP;

    // TCP checksum
    int tcp_len = sizeof(struct tcphdr);
    psh.tcp_length = htons(tcp_len);

    char pseudo_packet[PACKET_LEN];
    memset(pseudo_packet, 0, sizeof(pseudo_packet));
    memcpy(pseudo_packet, &psh, sizeof(psh));
    memcpy(pseudo_packet + sizeof(psh), tcph, tcp_len);
    tcph->th_sum = 0;
    tcph->th_sum = checksum((unsigned short *)pseudo_packet, sizeof(psh) + tcp_len);

    /*
    psh.tcp_length = htons(sizeof(struct tcphdr));

    char pseudo_packet[PACKET_LEN];
    memset(pseudo_packet, 0, PACKET_LEN);
    memcpy(pseudo_packet, &psh, sizeof(psh));
    memcpy(pseudo_packet + sizeof(psh), tcph, sizeof(struct tcphdr));
    tcph->th_sum = checksum((unsigned short *)pseudo_packet, sizeof(psh) + sizeof(struct tcphdr));
    */

    // Calculate IP checksum
    iph->ip_sum = checksum((unsigned short *)packet, sizeof(struct ip));

    // Set up pcap
/*
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle = pcap_open_live(iface, 65536, 0, 3000, errbuf);
    if (!handle) {
        fprintf(stderr, "pcap_open_live() failed: %s\n", errbuf);
        close(sock);
        exit(EXIT_FAILURE);
    }
*/
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle = pcap_create(iface, errbuf);
    pcap_set_snaplen(handle, 65536);
    pcap_set_timeout(handle, 1);  // 1ms timeout
    pcap_set_immediate_mode(handle, 1);
    pcap_activate(handle);
    // Filter
    char filter_exp[128];
    //"tcp and src host %s and (tcp[13] & 0x14 != 0)"
    //"dst host <out> and (imcp or imcp6 or (tcp or upd or sctp) and (src host %s))"
//    snprintf(filter_exp, sizeof(filter_exp), "tcp and src host %s and (tcp[13] & 0x14 != 0)", target_ip); // SYN/ACK or RST
    snprintf(filter_exp, sizeof(filter_exp), "tcp and src host %s and (tcp[13] & 0x14 != 0)", target_ip);
    struct bpf_program fp;
    if (pcap_compile(handle, &fp, filter_exp, 0, PCAP_NETMASK_UNKNOWN) < 0 ||
        pcap_setfilter(handle, &fp) < 0) {
        fprintf(stderr, "pcap filter error\n");
        pcap_close(handle);
        close(sock);
        exit(EXIT_FAILURE);
    }
    // Send packet - moved it down so that we would be better place to catch the answer directly (before the kernel)
    if (sendto(sock, packet, sizeof(struct ip) + sizeof(struct tcphdr), 0, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
        perror("sendto failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("[*] SYN packet sent to %s:%d\n", target_ip, port);
    // Capture response
    struct pcap_pkthdr *header;
    const u_char *data;
    int result;
    time_t start = time(NULL);
    while (1) {
        result = pcap_next_ex(handle, &header, &data);
        if (result == 1) {
            struct ip *rip = (struct ip *)(data + 14); // skip Ethernet
            struct tcphdr *rtcp = (struct tcphdr *)((u_char *)rip + (rip->ip_hl << 2));
            if (rtcp->th_flags & TH_SYN && rtcp->th_flags & TH_ACK) {
                printf("[+] Port %d is OPEN (SYN+ACK)\n", port);
                break;
            } else if (rtcp->th_flags & TH_RST) {
                printf("[-] Port %d is CLOSED (RST)\n", port);
                break;
            }
        }
    }

    pcap_close(handle);
    close(sock);
    return 0;
}
