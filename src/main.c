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

// Get the default interface and IP
int get_local_ip_and_iface(char *iface, char *ip) {
    struct ifaddrs *ifaddr, *ifa;
    int family;
    if (getifaddrs(&ifaddr) == -1) return -1;                           //if it succeeds, ifaddr is populated with a list of the interfaces
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {              //loops through the interfaces list
        if (ifa->ifa_addr == NULL) continue;                            //skip an interface if it doesn't have an address
        family = ifa->ifa_addr->sa_family;                              //check if the address is ipv4, ipv6, etc...
        if ((family == AF_INET) && !(ifa->ifa_flags & IFF_LOOPBACK)) {  //select one that's ipv4, excluding the loopback interface
            //stores the ip and interface in iface and ip (after).
            strcpy(iface, ifa->ifa_name);
            getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), ip, INET_ADDRSTRLEN, NULL, 0, NI_NUMERICHOST);
            freeifaddrs(ifaddr);                                        //free the result of getifaddrs()//NEED to check that the free was successful
            return 0;
        }
    }
    freeifaddrs(ifaddr);
    return -1;
}

int main(int argc, char *argv[]) {
// PARSING
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <target_ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    const char *const target_ip = argv[1];
    const int base_port = atoi(argv[2]);

// SETUP
    // Retrieve the interface and ip we will use
    char iface[IFNAMSIZ], local_ip[INET_ADDRSTRLEN];
    if (get_local_ip_and_iface(iface, local_ip) != 0) {
        perror("Could not determine local IP/interface");
        exit(EXIT_FAILURE);
    }
    printf("[*] Using interface: %s (%s)\n", iface, local_ip);

    // Create the raw socket
    const int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);// returns a file descriptor
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set IP_HDRINCL which tells the kernel not to add an header (since we do it ourselves)
    const int one = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
    //            (fd  , protocol  , option    , value, size      )
        perror("Error setting IP_HDRINCL");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Set up pcap
    char errbuf[PCAP_ERRBUF_SIZE];                      // will store pcap errors
    pcap_t *const handle = pcap_create(iface, errbuf);  // create a pcap capture handle (takes the interface and the error buffer)
    //pcap_set_snaplen(handle, 65536);                  // set the max len of the packets. Good to set but the default is usually the same
    //pcap_set_timeout(handle, 100); for now redundant, because we're in immediate mode and we handle the timeouts later in the code
    pcap_set_immediate_mode(handle, 1);                 // set the immediate mode. NEED TO LOOK AT LATER TO DECIDE WHICH IS BETTER
    pcap_activate(handle);                              // applies settings & activates the capture


// SET UP PCAP FILTER
    char filter_exp[256];
    snprintf(filter_exp, sizeof(filter_exp), "tcp and src host %s", target_ip); // simple filter, good enough for now but ideally it would be more specific
    printf("[*] Using filter: %s\n", filter_exp);
    struct bpf_program fp;
    if (pcap_compile(handle, &fp, filter_exp, 0, PCAP_NETMASK_UNKNOWN) < 0 ||
        pcap_setfilter(handle, &fp) < 0) {
        fprintf(stderr, "pcap filter error\n");
        pcap_close(handle);
        close(sock);
        exit(EXIT_FAILURE);
    }

// SENDING THE PACKETS
    printf("[*] Sending SYN packets to ports %d-%d...\n", base_port, base_port + 9);
    
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
        const int source_port = 32768 + rand() % (65535 - 32768);
        tcph->th_sport = htons(source_port);
        tcph->th_dport = htons(port);
//       printf("[DEBUG] Sending SYN from port %d to port %d\n", source_port, port);
        tcph->th_seq = htonl(i);
        tcph->th_ack = 0;
        tcph->th_off = 5;
        tcph->th_flags = TH_SYN;
        tcph->th_win = htons(1024);
        tcph->th_sum = 0;
        tcph->th_urp = 0;

        // Calculate TCP checksum
        struct s_pseudo_header psh;
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

        // Send packet
        if (sendto(sock, packet, sizeof(struct ip) + sizeof(struct tcphdr), 0, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
            perror("sendto failed");
            close(sock);
            pcap_close(handle);
            exit(EXIT_FAILURE);
        }
        printf("[*] SYN packet sent to %s:%d\n", target_ip, port);
    }

// LISTEN FOR RESPONSES
    printf("[*] Listening for responses...\n\n");
    
    // Track which ports we've received responses for
    int responses_received[10] = {0};
    int total_responses = 0;
    
    const int pcap_fd = pcap_get_selectable_fd(handle);
    time_t start = time(NULL);
    
    while (total_responses < 10) {
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
            struct pcap_pkthdr *header;
            const u_char *data;
            const int result = pcap_next_ex(handle, &header, &data);
            
            if (result == 1) {
                struct ip *rip = (struct ip *)(data + 14); // skip Ethernet
                struct tcphdr *rtcp = (struct tcphdr *)((u_char *)rip + (rip->ip_hl << 2));
                
                // Debug: Print all captured packets
//                printf("[DEBUG] Received packet: src_port=%d, dst_port=%d, flags=0x%02x\n", 
//                       ntohs(rtcp->th_sport), ntohs(rtcp->th_dport), rtcp->th_flags);
                
                // Find which port this response corresponds to
                const int response_port = ntohs(rtcp->th_sport);
                const int port_index = response_port - base_port;
                /*This part receives the unique scan id
                uint32_t scan_id = ntohl(rtcp->th_ack) - 1;
                printf("[DEBUG] Received response for scan ID: %u\n", scan_id);
                
                //*/
                
//                printf("[DEBUG] Response port: %d, port_index: %d\n", response_port, port_index);
                
                // Check if this is a response for one of our scanned ports
                if (port_index >= 0 && port_index < 10 && !responses_received[port_index]) {
//                    printf("[DEBUG] Processing response for port %d\n", response_port);
                    if (rtcp->th_flags & TH_SYN && rtcp->th_flags & TH_ACK) {
                        printf("[+] Port %d is OPEN (SYN+ACK)\n", response_port);
                        responses_received[port_index] = 1;
                        total_responses++;
                    } else if (rtcp->th_flags & TH_RST) {
                        printf("[-] Port %d is CLOSED (RST)\n", response_port);
                        responses_received[port_index] = 1;
                        total_responses++;
                    } else {
//                        printf("[DEBUG] Unexpected flags: 0x%02x\n", rtcp->th_flags);
                    }
                } else {
//                    printf("[DEBUG] Skipping response - port_index: %d, already_received: %d\n", 
//                           port_index, (port_index >= 0 && port_index < 10) ? responses_received[port_index] : -1);
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
    printf("[*] Scan complete. Reporting filtered ports:\n");
    for (int i = 0; i < 10; ++i) {
        if (!responses_received[i]) {
            printf("[?] Port %d is FILTERED or no response\n", base_port + i);
        }
    }

    pcap_close(handle);
    close(sock);
    return 0;
}