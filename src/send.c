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

uint8_t get_flags(const scan_type_t scan_type) {
    switch (scan_type) {
        case SCAN_TYPE_SYN:
            return TH_SYN;
        case SCAN_TYPE_NULL:
            return 0;
        case SCAN_TYPE_ACK:
            return TH_ACK;
        case SCAN_TYPE_FIN:
            return TH_FIN;
        case SCAN_TYPE_XMAS:
            return TH_FIN | TH_PUSH | TH_URG; // XMAS flags
        default:
            return 0;
    }
}

void fill_ip_header(struct ip *iph, const char *local_ip, const struct sockaddr_in *dest) {
    iph->ip_hl = sizeof(struct ip) / 4;             // Header length
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
    iph->ip_p = IPPROTO_TCP;                        // Protocol (TCP)
    iph->ip_src.s_addr = inet_addr(local_ip);       // Source IP
    iph->ip_dst = dest->sin_addr;                   // Destination IP
}

void fill_tcp_header(struct tcphdr *tcph, const probe_t *p) {
    tcph->th_sport = htons(DEFAULT_PORT + p->scan_type);        // Source port
    tcph->th_dport = p->address.sin_port;                          // Destination port
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

    memcpy(pseudo_packet, &psh, sizeof(psh));                            // Copy the pseudo header into the pseudo packet
    memcpy(pseudo_packet + sizeof(psh), tcph, sizeof(struct tcphdr));    // Append the TCP header
}
//
void send_udp_packet(const char *local_ip, const int sock, const struct sockaddr_in destination);
//
void send_packet(const char *local_ip, const int sock, const probe_t p) {
    if (p.scan_type == SCAN_TYPE_UDP) {
        send_udp_packet(local_ip, sock, p.address);
        return;
    }
    // Allocate only as much as needed for headers
    char packet[sizeof(struct ip) + sizeof(struct tcphdr)]; // The size is just was we need for the headers
    memset(packet, 0, sizeof(packet));
    struct ip *const iph = (struct ip *) packet;
    struct tcphdr *const tcph = (struct tcphdr *) (packet + sizeof(struct ip));

    // Prepare the destination address
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
#if defined(__APPLE__)
    dest.sin_len = sizeof(struct sockaddr_in);
#endif
    dest.sin_family = AF_INET;                      // IPv4
    dest.sin_port = p.address.sin_port;                // Convert the port to the network byte order
    dest.sin_addr = p.address.sin_addr;                // Convert the target IP to binary form and assign it directly

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

static size_t payload_len [[maybe_unused]] (udp_packet_t packet)
{
    return (size_t)ntohs(packet.udp_header.uh_ulen);
}

static size_t packet_len(udp_packet_t packet)
{
    return (size_t)ntohs(packet.ip_header.ip_len);
}

udp_packet_t get_udp_packet_checksumed(const udp_packet_t unchecksumed_packet)
{
    udp_packet_t packet = unchecksumed_packet;

    // Derive lengths safely
    const uint16_t udp_length_network = packet.udp_header.uh_ulen;
    const uint16_t udp_length = ntohs(udp_length_network);

    const uint16_t udp_header_len = (uint16_t)sizeof(struct udphdr);
    const uint16_t max_payload_len = (uint16_t)sizeof(packet.payload);

    const uint16_t raw_payload_len =
        (udp_length > udp_header_len) ? (uint16_t)(udp_length - udp_header_len) : (uint16_t)0u;
    const uint16_t payload_len = (raw_payload_len > max_payload_len) ? max_payload_len : raw_payload_len;

    // Build IPv4 pseudo header
    const t_pseudo_header psh = (t_pseudo_header){
        .src = packet.ip_header.ip_src.s_addr,
        .dst = packet.ip_header.ip_dst.s_addr,
        .reserved = 0u,
        .protocol = IPPROTO_UDP,
        .tcp_length = htons(udp_header_len + payload_len),
    };

    // Prepare contiguous buffer: pseudo header + UDP header + payload
    enum
    {
        PSEUDO_BYTES = (int)sizeof(t_pseudo_header)
    };
    enum
    {
        UDP_SEGMENT_MAX = (int)sizeof(struct udphdr) + (int)sizeof(((udp_packet_t *)0)->payload)
    };
    unsigned char pseudo_packet[PSEUDO_BYTES + UDP_SEGMENT_MAX];
    memset(pseudo_packet, 0, sizeof(pseudo_packet));

    // Copy pseudo header
    memcpy(pseudo_packet, &psh, sizeof(psh));

    // Copy UDP header with checksum set to zero for calculation
    struct udphdr udp_header = packet.udp_header;
    udp_header.uh_sum = 0;
    memcpy(pseudo_packet + sizeof(psh), &udp_header, sizeof(struct udphdr));

    // Copy payload actually present
    if (payload_len > 0)
    {
        memcpy(pseudo_packet + sizeof(psh) + sizeof(struct udphdr), packet.payload, payload_len);
    }

    // Compute checksum over pseudo header + UDP header + payload
    const int checksum_bytes = (int)sizeof(psh) + (int)sizeof(struct udphdr) + (int)payload_len;
    const unsigned short udp_sum = checksum((unsigned short *)pseudo_packet, checksum_bytes);

    packet.udp_header.uh_sum = udp_sum;

    return packet;
}

struct ip get_ip_header_checksumed(struct ip unchecksumed_header)
{
    struct ip header = unchecksumed_header;
    header.ip_sum = checksum((unsigned short *)&header, (int)sizeof(struct ip));
    return header;
}

udp_packet_t build_udp_packet(struct sockaddr_in destination, const char *local_ip)
{
    const size_t payload_len =
        (destination.sin_port == 53 || destination.sin_port == 161) ? sizeof(((udp_packet_t *)0)->payload) : 0;

    udp_packet_t packet =
        (udp_packet_t){.ip_header = get_ip_header_checksumed((struct ip){
                           .ip_v = 4,
                           .ip_hl = OCTET232BITWORD(sizeof(struct ip)),
                           .ip_tos = 0, // DSCP and ECN
                           .ip_id = htons(0),
                           .ip_off = htons(0),                        // Flags and fragment offset
                           .ip_ttl = 64,                              // time to live
                           .ip_p = IPPROTO_UDP,                       // protocol
                           .ip_sum = 0,                               // checksum
                           .ip_src = {.s_addr = inet_addr(local_ip)}, // set source ip
                           .ip_dst = destination.sin_addr,            // set destination ip
                           .ip_len = htons(sizeof(struct ip) + sizeof(struct udphdr) + (u_short)payload_len),
                       }),
                       .udp_header =
                           {
                               .uh_sport = htons(DEFAULT_PORT + SCAN_TYPE_UDP),                          // set source port
                               .uh_dport = destination.sin_port,                        // set destination port
                               .uh_ulen = htons(sizeof(struct udphdr) + (u_short)payload_len), // set length
                               .uh_sum = 0, // set checksum optional on IPv4
                           },
                       .payload = {0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10}};
    return packet;
}

void send_udp_packet(const char *local_ip, const int sock, const struct sockaddr_in destination)
{
    udp_packet_t packet = build_udp_packet(destination, local_ip);
    sendto(sock, &packet, packet_len(packet), 0, (struct sockaddr *)&destination, sizeof(destination));
}

/*
void send_udp_packet(const char *local_ip, const int sock, const probe_t p)
{
    const struct sockaddr_in destination = p.address;
    udp_packet_t packet = build_udp_packet(destination, local_ip);
    sendto(sock, &packet, packet_len(packet), 0, (struct sockaddr *)&destination, sizeof(destination));
}
23:23:29.286696 IP scanme.nmap.org > 172.21.155.86: ICMP scanme.nmap.org udp port 20480 unreachable, length 36
23:23:29.286697 IP scanme.nmap.org > 172.21.155.86: ICMP scanme.nmap.org udp port 20736 unreachable, length 36
*/
