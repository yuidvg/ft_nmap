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

void send_syn_packet(const char *local_ip, const int sock, const struct sockaddr_in test, const uint16_t i) {
    //printf("[*] Sending SYN packets to ports %d-%d...\n", base_port, base_port + 9);

    const struct in_addr target_ip = test.sin_addr; // Get the target IP from the probe
    const uint16_t port = test.sin_port; // Get the port from the probe
    // Allocate only as much as needed for headers
    char packet[sizeof(struct ip) + sizeof(struct tcphdr)]; // The size is just was we need for the headers
    ft_memset(packet, 0, sizeof(packet));
    struct ip *const iph = (struct ip *) packet;
    struct tcphdr *const tcph = (struct tcphdr *) (packet + sizeof(struct ip));

    // Prepare the destination address
    struct sockaddr_in dest;
    dest.sin_family = AF_INET;                      // IPv4
    dest.sin_port = port;                    // Convert the port to the network byte order
    dest.sin_addr = target_ip;    // Convert the target IP to binary form and assign it directly

    // IP header - Contains the IP addresses and other relevant info
    iph->ip_hl = sizeof(struct ip) / 4;             // Header length
    iph->ip_v = 4;                                  // IP version (IPv4)
    iph->ip_tos = 0;                                // Type of Service [is it useful to initialize it?]
    iph->ip_len = htons(sizeof(struct ip) + sizeof(struct tcphdr));
    iph->ip_id = htons(i);                          // Unique ID (not sent back unlike seq)
    iph->ip_off = 0;                                // Fragment offset (no fragmentation so useless)
    iph->ip_ttl = 64;                               // Time to Live (max number of hops - default)
    iph->ip_p = IPPROTO_TCP;                        // Protocol (TCP)
    iph->ip_src.s_addr = inet_addr(local_ip);       // Source IP
    iph->ip_dst = dest.sin_addr;                    // Destination IP

    // Fill TCP header - Contains the ports, flags and other relevant info
    const uint16_t source_port = 32768 + rand() % (65535 - 32768);   // Random source port [We should ensure it's not already in use]
    tcph->th_sport = htons(source_port);                        // Source port
    tcph->th_dport = port;                               // Destination port
    tcph->th_seq = htonl(i);                                    // Unique sequence number (used to identify the responses)
    tcph->th_ack = 0;                                           // Acknowledgment (not used in SYN scans)
    tcph->th_off = sizeof(struct tcphdr) / 4;                   // Payload offset
    tcph->th_flags = TH_SYN;                                    // Set SYN flag
    tcph->th_win = htons(1024);                                 // Window size (default - not really relevent since we're not really sending data)
    tcph->th_urp = 0;                                           // Urgent pointer (rarely used - not relevant here)

//       printf("[DEBUG] Sending SYN from port %d to port %d\n", source_port, port);

    // Calculate TCP checksum - Calculated over the tcp header, the data, and parts of the IP header (hence the need for a pseudo header)
    t_pseudo_header psh;                     // The pseudo header
    psh.src = iph->ip_src.s_addr;                   // Source IP address
    psh.dst = iph->ip_dst.s_addr;                   // Destination IP address
    psh.reserved = 0;                               // Always 0
    psh.protocol = IPPROTO_TCP;                     // Protocol type (TCP)
    psh.tcp_length = htons(sizeof(struct tcphdr));

    // Create a pseudo packet - Pseudo header + TCP header
    char pseudo_packet[sizeof(t_pseudo_header) + sizeof(struct tcphdr)];
    ft_memset(pseudo_packet, 0, sizeof(pseudo_packet));                    // Initialization
    ft_memcpy(pseudo_packet, &psh, sizeof(psh));                           // Copy the pseudo header into the pseudo packet
    ft_memcpy(pseudo_packet + sizeof(psh), tcph, sizeof(struct tcphdr));   // Append the TCP header

    tcph->th_sum = 0;
    tcph->th_sum = checksum((unsigned short *)pseudo_packet, sizeof(psh) + sizeof(struct tcphdr));

    // Calculate IP checksum - Calculated over the IP header only
    iph->ip_sum = 0;
    iph->ip_sum = checksum((unsigned short *)packet, sizeof(struct ip));

    // Send packet
    if (sendto(sock, packet, sizeof(struct ip) + sizeof(struct tcphdr), 0, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
        perror("sendto failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    if (DEBUG) printf("[*] SYN packet sent to %s:%d\n", inet_ntoa(dest.sin_addr), ntohs(dest.sin_port));
    //sleep 1 ms 
    usleep(100);//removing this line causes the program to miss some responses when there are threads
}