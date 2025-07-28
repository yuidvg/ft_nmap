#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pcap.h>
//#include <pcap/pcap.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <time.h>

// IP and TCP header lengths
#define IP4_HDRLEN 20
#define TCP_HDRLEN 20

// Calculate checksum
unsigned short checksum(unsigned short *ptr, int nbytes) {
    long sum;
    unsigned short oddbyte;
    unsigned short answer;

    sum = 0;
    while (nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }

    if (nbytes == 1) {
        oddbyte = 0;
        *((u_char *) &oddbyte) = *(u_char *) ptr;
        sum += oddbyte;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = (unsigned short) ~sum;

    return answer;
}

// Pseudo-header for TCP checksum
struct pseudo_header {
    u_int32_t src;
    u_int32_t dst;
    u_int8_t reserved;
    u_int8_t protocol;
    u_int16_t tcp_length;
};

void build_tcp_syn_packet(char *packet, const char *src_ip, const char *dst_ip, int src_port, int dst_port) {
    struct ip *iph = (struct ip *) packet;
    struct tcphdr *tcph = (struct tcphdr *) (packet + IP4_HDRLEN);

    memset(packet, 0, IP4_HDRLEN + TCP_HDRLEN);

    // IP header
    iph->ip_hl = 5;
    iph->ip_v = 4;
    iph->ip_tos = 0;
    iph->ip_len = htons(IP4_HDRLEN + TCP_HDRLEN);
    iph->ip_id = htons(rand() % 65535);
    iph->ip_off = 0;
    iph->ip_ttl = 64;
    iph->ip_p = IPPROTO_TCP;
    iph->ip_sum = 0;
    iph->ip_src.s_addr = inet_addr(src_ip);
    iph->ip_dst.s_addr = inet_addr(dst_ip);
    iph->ip_sum = checksum((unsigned short *) iph, IP4_HDRLEN);

    // TCP header
    tcph->th_sport = htons(src_port);
    tcph->th_dport = htons(dst_port);
    tcph->th_seq = htonl(rand());
    tcph->th_off = 5;
    tcph->th_flags = TH_SYN;
    tcph->th_win = htons(65535);
    tcph->th_sum = 0;

    // Pseudo-header for checksum
    struct pseudo_header psh;
    psh.src = iph->ip_src.s_addr;
    psh.dst = iph->ip_dst.s_addr;
    psh.reserved = 0;
    psh.protocol = IPPROTO_TCP;
    psh.tcp_length = htons(TCP_HDRLEN);

    int psize = sizeof(struct pseudo_header) + TCP_HDRLEN;
    char *pseudogram = malloc(psize);
    memcpy(pseudogram, &psh, sizeof(struct pseudo_header));
    memcpy(pseudogram + sizeof(struct pseudo_header), tcph, TCP_HDRLEN);

    tcph->th_sum = checksum((unsigned short *) pseudogram, psize);
    free(pseudogram);
}

void send_syn_packet(const char *src_ip, const char *dst_ip, int src_port, int dst_port) {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (sock < 0) {
        perror("Raw socket");
        exit(1);
    }

    int one = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("IP_HDRINCL");
        exit(1);
    }

    char packet[IP4_HDRLEN + TCP_HDRLEN];
    build_tcp_syn_packet(packet, src_ip, dst_ip, src_port, dst_port);

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(dst_port);
    dest.sin_addr.s_addr = inet_addr(dst_ip);

    if (sendto(sock, packet, IP4_HDRLEN + TCP_HDRLEN, 0, (struct sockaddr *) &dest, sizeof(dest)) < 0) {
        perror("sendto");
        exit(1);
    }

    close(sock);
}

// libpcap packet handler
void packet_handler(u_char *user, const struct pcap_pkthdr *h, const u_char *bytes) {
    struct ip *iph = (struct ip *)(bytes + 14);  // Ethernet header is 14 bytes
    if (iph->ip_p != IPPROTO_TCP) return;

    struct tcphdr *tcph = (struct tcphdr *) ((u_char *)iph + (iph->ip_hl * 4));

    if (tcph->th_flags & TH_SYN && tcph->th_flags & TH_ACK) {
        printf("Port %d is OPEN (SYN+ACK)\n", ntohs(tcph->th_sport));
    } else if (tcph->th_flags & TH_RST) {
        printf("Port %d is CLOSED (RST)\n", ntohs(tcph->th_sport));
    }
}

void sniff_response(const char *filter_expr, const char *iface) {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle = pcap_open_live(iface, BUFSIZ, 0, 1000, errbuf);
    if (!handle) {
        fprintf(stderr, "pcap_open_live failed: %s\n", errbuf);
        exit(1);
    }

    struct bpf_program fp;
    if (pcap_compile(handle, &fp, filter_expr, 0, PCAP_NETMASK_UNKNOWN) == -1) {
        fprintf(stderr, "pcap_compile error\n");
        exit(1);
    }
    if (pcap_setfilter(handle, &fp) == -1) {
        fprintf(stderr, "pcap_setfilter error\n");
        exit(1);
    }

    printf("Listening for replies...\n");
    pcap_dispatch(handle, 10, packet_handler, NULL);
    pcap_close(handle);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s <interface> <source_ip> <target_ip> <port>\n", argv[0]);
        return 1;
    }

    srand(time(NULL));
    const char *iface = argv[1];
    const char *src_ip = argv[2];
    const char *dst_ip = argv[3];
    int dst_port = atoi(argv[4]);
    int src_port = 54321;

    send_syn_packet(src_ip, dst_ip, src_port, dst_port);

    char filter[128];
    snprintf(filter, sizeof(filter), "tcp and src host %s and dst port %d", dst_ip, src_port);

    sniff_response(filter, iface);

    return 0;
}
