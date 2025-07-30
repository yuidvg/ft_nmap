#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/select.h>

#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netdb.h>
#include <ifaddrs.h>

#include <pcap.h>

#define PACKET_LEN 4096

// IP and TCP headers
struct pseudo_header {
    u_int32_t src;
    u_int32_t dst;
    u_int8_t reserved;
    u_int8_t protocol;
    u_int16_t tcp_length;
};

// Checksum function - verify if the content was corrupted
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
    if (getifaddrs(&ifaddr) == -1) return -1;                           //if it succeeds, ifaddr is populated with a list of the interfaces
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {              //loops through the interfaces list
        if (ifa->ifa_addr == NULL) continue;                            //skip an interface if it doesn't have an address
        family = ifa->ifa_addr->sa_family;                              //check if the address is ipv4, ipv6, etc...
        if ((family == AF_INET) && !(ifa->ifa_flags & IFF_LOOPBACK)) {  //select one that's ipv4, excluding the loopback interface
            //stors the ip and interface in iface and ip (after).
            strcpy(iface, ifa->ifa_name);
            getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), ip, INET_ADDRSTRLEN, NULL, 0, NI_NUMERICHOST);
            freeifaddrs(ifaddr);//free the result of getifaddrs()//NEED to check that the free was successful
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
// ... existing code up to line 77 ...

const char *const target_ip = argv[1];
const int base_port = atoi(argv[2]);

char iface[IFNAMSIZ], local_ip[INET_ADDRSTRLEN];
if (get_local_ip_and_iface(iface, local_ip) != 0) {
    perror("Could not determine local IP/interface");
    exit(EXIT_FAILURE);
}
printf("[*] Using interface: %s (%s)\n", iface, local_ip);

// Raw socket
const int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
if (sock < 0) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
}

// Set IP_HDRINCL
const int one = 1;
if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
    perror("Setting IP_HDRINCL");
    close(sock);
    exit(EXIT_FAILURE);
}

    // Set up pcap once
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *const handle = pcap_create(iface, errbuf);
    pcap_set_snaplen(handle, 65536);
    pcap_set_timeout(handle, 100);  // 100ms timeout
    pcap_set_immediate_mode(handle, 1);
    pcap_activate(handle);

for (int i = 0; i < 10; ++i) {
    const int port = base_port + i;

    // Packet buffer
    char packet[PACKET_LEN];
    memset(packet, 0, PACKET_LEN);

    struct ip *const iph = (struct ip *) packet;
    struct tcphdr *const tcph = (struct tcphdr *) (packet + sizeof(struct ip));

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
    const int tcp_len = sizeof(struct tcphdr);
    psh.tcp_length = htons(tcp_len);

    char pseudo_packet[PACKET_LEN];
    memset(pseudo_packet, 0, sizeof(pseudo_packet));
    memcpy(pseudo_packet, &psh, sizeof(psh));
    memcpy(pseudo_packet + sizeof(psh), tcph, tcp_len);
    tcph->th_sum = 0;
    tcph->th_sum = checksum((unsigned short *)pseudo_packet, sizeof(psh) + tcp_len);

    // Calculate IP checksum
    iph->ip_sum = checksum((unsigned short *)packet, sizeof(struct ip));

    // Set up filter for this port
    char filter_exp[128];
    snprintf(filter_exp, sizeof(filter_exp), "tcp and src host %s and dst port %d and (tcp[13] & 0x14 != 0)", target_ip, ntohs(tcph->th_sport));
    struct bpf_program fp;
    if (pcap_compile(handle, &fp, filter_exp, 0, PCAP_NETMASK_UNKNOWN) < 0 ||
        pcap_setfilter(handle, &fp) < 0) {
        fprintf(stderr, "pcap filter error\n");
        pcap_close(handle);
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Send packet
    if (sendto(sock, packet, sizeof(struct ip) + sizeof(struct tcphdr), 0, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
        perror("sendto failed");
        close(sock);
        pcap_close(handle);
        exit(EXIT_FAILURE);
    }
    printf("[*] SYN packet sent to %s:%d\n", target_ip, port);

    // Capture response
    struct pcap_pkthdr *header;
    const u_char *data;
    int result;
    time_t start = time(NULL);
    int found = 0;
    
    // Get pcap file descriptor for select
    const int pcap_fd = pcap_get_selectable_fd(handle);
    
    while (time(NULL) - start < 2) { // 2 second timeout per port
        fd_set read_fds;
        struct timeval timeout;
        
        FD_ZERO(&read_fds);
        FD_SET(pcap_fd, &read_fds);
        
        // Calculate remaining time
        const time_t elapsed = time(NULL) - start;
        const time_t remaining = 2 - elapsed;
        if (remaining <= 0) break;
        
        timeout.tv_sec = remaining;
        timeout.tv_usec = 0;
        
        // Use select to wait for data with timeout
        const int select_result = select(pcap_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (select_result > 0 && FD_ISSET(pcap_fd, &read_fds)) {
            // Data available, try to read it
            result = pcap_next_ex(handle, &header, &data);
            if (result == 1) {
                struct ip *rip = (struct ip *)(data + 14); // skip Ethernet
                struct tcphdr *rtcp = (struct tcphdr *)((u_char *)rip + (rip->ip_hl << 2));
                if (rtcp->th_flags & TH_SYN && rtcp->th_flags & TH_ACK) {
                    printf("[+] Port %d is OPEN (SYN+ACK)\n", port);
                    found = 1;
                    break;
                } else if (rtcp->th_flags & TH_RST) {
                    printf("[-] Port %d is CLOSED (RST)\n", port);
                    found = 1;
                    break;
                }
            } else if (result == -1) {
                // Error occurred
                fprintf(stderr, "Error reading packet: %s\n", pcap_geterr(handle));
                break;
            }
        } else if (select_result == 0) {
            // Timeout occurred
            break;
        } else if (select_result < 0) {
            // Select error
            perror("select() failed");
            break;
        }
    }
    if (!found) {
        printf("[?] Port %d is FILTERED or no response\n", port);
    }
}

pcap_close(handle);
close(sock);
return 0;
}