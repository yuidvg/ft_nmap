# include <../include/all.h>

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

uint8_t get_flags(const t_scan_types scan_type) {
    switch (scan_type) {
        case SYN:
            return TH_SYN;
        case NUL:
            return 0;
        case ACK:
            return TH_ACK;
        case FIN:
            return TH_FIN;
        case XMAS:
            return TH_FIN | TH_PUSH | TH_URG; // XMAS flags
        default:
            return 0;
    }
}

void fill_ip_header(struct ip *iph, const char *local_ip, const struct sockaddr_in *dest) {
    iph->ip_hl = sizeof(struct ip) / 4;             // Header length
    iph->ip_v = 4;                                  // IP version (IPv4)
    iph->ip_tos = 0;                                // Type of Service
    iph->ip_len = htons(sizeof(struct ip) + sizeof(struct tcphdr));
    iph->ip_id = rand() % 65536;                    // Unique ID (not sent back unlike seq)
    iph->ip_off = 0;                                // Fragment offset (no fragmentation so useless)
    iph->ip_ttl = 64;                               // Time to Live (max number of hops - default)
    iph->ip_p = IPPROTO_TCP;                        // Protocol (TCP)
    iph->ip_src.s_addr = inet_addr(local_ip);       // Source IP
    iph->ip_dst = dest->sin_addr;                   // Destination IP
}

void fill_tcp_header(struct tcphdr *tcph, const t_probe *p) {
    tcph->th_sport = htons(DEFAULT_PORT + p->scan_type);        // Source port
    tcph->th_dport = p->addr.sin_port;                          // Destination port
    tcph->th_seq = rand();                                      // Unique sequence number (used to identify the responses)
    tcph->th_ack = 0;
    tcph->th_off = sizeof(struct tcphdr) / 4;                   // Payload offset
    tcph->th_flags = get_flags(p->scan_type);
    tcph->th_win = htons(1024);                                 // Window size (default - not really relevent since we're not really sending data)
    tcph->th_urp = 0;                                           // Urgent pointer (rarely used - not relevant here)
}

void fill_pseudo_packet(char *pseudo_packet, const struct ip *iph, const struct tcphdr *tcph) {
    t_pseudo_header psh;
    psh.src = iph->ip_src.s_addr;                   // Source IP address
    psh.dst = iph->ip_dst.s_addr;                   // Destination IP address
    psh.reserved = 0;                               // Reserved field (always 0)
    psh.protocol = IPPROTO_TCP;                     // Protocol type (TCP)
    psh.tcp_length = htons(sizeof(struct tcphdr)); 

    ft_memcpy(pseudo_packet, &psh, sizeof(psh));                            // Copy the pseudo header into the pseudo packet
    ft_memcpy(pseudo_packet + sizeof(psh), tcph, sizeof(struct tcphdr));    // Append the TCP header
}

void send_packet(const char *local_ip, const int sock, const t_probe p) {
    // Allocate only as much as needed for headers
    char packet[sizeof(struct ip) + sizeof(struct tcphdr)]; // The size is just was we need for the headers
    ft_memset(packet, 0, sizeof(packet));
    struct ip *const iph = (struct ip *) packet;
    struct tcphdr *const tcph = (struct tcphdr *) (packet + sizeof(struct ip));

    // Prepare the destination address
    struct sockaddr_in dest;
    dest.sin_family = AF_INET;                      // IPv4
    dest.sin_port = p.addr.sin_port;                // Convert the port to the network byte order
    dest.sin_addr = p.addr.sin_addr;                // Convert the target IP to binary form and assign it directly

    fill_ip_header(iph, local_ip, &dest);   // IP header - Contains the IP addresses and other relevant info
    fill_tcp_header(tcph, &p);              // Fill TCP header - Contains the ports, flags and other relevant info

    // Calculate TCP checksum - Calculated over the tcp header, the data, and parts of the IP header (hence the need for a pseudo header)
    char pseudo_packet[sizeof(t_pseudo_header) + sizeof(struct tcphdr)];
    fill_pseudo_packet(pseudo_packet, iph, tcph);
    tcph->th_sum = checksum((unsigned short *)pseudo_packet, sizeof(t_pseudo_header) + sizeof(struct tcphdr));
    // Calculate IP checksum - Calculated over the IP header only
    iph->ip_sum = checksum((unsigned short *)packet, sizeof(struct ip));

    // Send packet
    if (sendto(sock, packet, sizeof(struct ip) + sizeof(struct tcphdr), 0, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
        perror("sendto failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    if (DEBUG) printf("[*] %d packet sent to %s:%d\n", (int)p.scan_type, inet_ntoa(dest.sin_addr), ntohs(dest.sin_port));
    usleep(10000); // 10ms
}