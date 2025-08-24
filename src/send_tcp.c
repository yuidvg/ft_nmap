# include "internal.h"

uint8_t get_tcp_flags(const scan_type_t scan_type) {
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

void fill_tcp_header(struct tcphdr *tcph, const struct ip *iph, const struct sockaddr_in *dest, scan_type_t scan_type) {
    tcph->th_sport = htons(DEFAULT_PORT + scan_type);           // Source port
    tcph->th_dport = dest->sin_port;                            // Destination port
    tcph->th_seq = rand();                                      // Unique sequence number (used to identify the responses)
    tcph->th_ack = 0;
    tcph->th_off = sizeof(struct tcphdr) / 4;                   // Payload offset
    tcph->th_flags = get_tcp_flags(scan_type);
    tcph->th_win = htons(1024);                                 // Window size (default - not really relevent since we're not really sending data)
    tcph->th_urp = 0;                                           // Urgent pointer (rarely used - not relevant here)
    
    // Calculate TCP checksum - Calculated over the tcp header, the data, and parts of the IP header (hence the need for a pseudo header)
    char pseudo_packet[sizeof(t_pseudo_header) + sizeof(struct tcphdr)];
    fill_pseudo_packet(pseudo_packet, iph, tcph, sizeof(struct tcphdr), IPPROTO_TCP);
    tcph->th_sum = checksum((unsigned short *)pseudo_packet, sizeof(t_pseudo_header) + sizeof(struct tcphdr));

}

void send_tcp_packet(const char *local_ip, const int sock, const struct sockaddr_in dest, scan_type_t scan_type) {
    // Allocate only as much as needed for headers
    char packet[sizeof(struct ip) + sizeof(struct tcphdr)]; // The size is just was we need for the headers
    memset(packet, 0, sizeof(packet));

    struct ip *const iph = (struct ip *) packet;
    struct tcphdr *const tcph = (struct tcphdr *) (packet + sizeof(struct ip));

    fill_ip_header(iph, local_ip, &dest, IPPROTO_TCP);  // IP header - Contains the IP addresses and other relevant info
    fill_tcp_header(tcph, iph, &dest, scan_type);   // Fill TCP header - Contains the ports, flags and other relevant info

    // Send packet
    if (sendto(sock, packet, sizeof(struct ip) + sizeof(struct tcphdr), 0, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
        perror("sendto failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
}