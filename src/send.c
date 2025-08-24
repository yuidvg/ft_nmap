# include "internal.h"

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

void fill_pseudo_packet(char *pseudo_packet, const struct ip *iph, const void *header, uint8_t header_size, int protocol) {
    t_pseudo_header psh;
    psh.src = iph->ip_src.s_addr;
    psh.dst = iph->ip_dst.s_addr;
    psh.reserved = 0;
    psh.protocol = protocol;
    psh.tcp_length = htons(header_size);

    memcpy(pseudo_packet, &psh, sizeof(psh));
    memcpy(pseudo_packet + sizeof(psh), header, header_size);
}

void fill_ip_header(struct ip *iph, const char *local_ip, const struct sockaddr_in *dest, int protocol) {
    iph->ip_hl = OCTET232BITWORD(sizeof(struct ip));// Header length
    iph->ip_v = 4;                                  // IP version (IPv4)
    iph->ip_tos = 0;                                // Type of Service
    // BSD/macOS expect ip_len and ip_off in host byte order with IP_HDRINCL, Linux expects network byte order. Handle both.
#if defined(__APPLE__)
    iph->ip_len = (uint16_t)(sizeof(struct ip) + sizeof(struct tcphdr));
#else
    iph->ip_len = htons(sizeof(struct ip) + sizeof(struct tcphdr));
#endif
    iph->ip_id = htons(rand() % 65536);             // Unique ID (not sent back unlike seq)
    iph->ip_off = 0;                                // Fragment offset (no fragmentation so useless)
    iph->ip_ttl = 64;                               // Time to Live (max number of hops - default)
    iph->ip_p = protocol;                           // Protocol (TCP)
    iph->ip_src.s_addr = inet_addr(local_ip);       // Source IP
    iph->ip_dst = dest->sin_addr;                   // Destination IP
    iph->ip_sum = 0;                                // Checksum is set to 0 because it's part of the calculation
    iph->ip_sum = checksum((unsigned short *)iph, sizeof(struct ip));
}

void send_packet(const char *local_ip, const int sock, const probe_t p) {
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));

#if defined(__APPLE__)
    dest.sin_len = sizeof(struct sockaddr_in);
#endif
    dest.sin_family = AF_INET;                         // IPv4
    dest.sin_port = p.address.sin_port;                // Convert the port to the network byte order
    dest.sin_addr = p.address.sin_addr;                // Convert the target IP to binary form and assign it directly

    if (p.scan_type == SCAN_TYPE_UDP) {
        send_udp_packet(local_ip, sock, dest);
        return;
    } else {
        send_tcp_packet(local_ip, sock, dest, p.scan_type);
    }
    usleep(10000); // 10ms
}