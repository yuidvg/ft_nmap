# include <../include/all.h>
# include <../libft/libft.h>

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
int task_index = 0;
int num_tasks = 0;
char iface[IFNAMSIZ], local_ip[INET_ADDRSTRLEN];
int sock;
pcap_t *handle;
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
    if (getifaddrs(&ifaddr) == -1) return -1;                           // If it succeeds, ifaddr is populated with a list of the interfaces
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {              // Loops through the interfaces list
        if (ifa->ifa_addr == NULL) continue;                            // Skip an interface if it doesn't have an address
        family = ifa->ifa_addr->sa_family;                              // Check if the address is ipv4, ipv6, etc...
        if ((family == AF_INET) && !(ifa->ifa_flags & IFF_LOOPBACK)) {  // Select one that's ipv4, excluding the loopback interface
            //stores the ip and interface in iface and ip (after).
            ft_strcpy(iface, ifa->ifa_name);
            getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), ip, INET_ADDRSTRLEN, NULL, 0, NI_NUMERICHOST);
            freeifaddrs(ifaddr);                                        // Free the result of getifaddrs()//NEED to check that the free was successful
            return 0;
        }
    }
    freeifaddrs(ifaddr);
    return -1;
}

int setup_socket() {
    // Create the raw socket
    const int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);// Returns a file descriptor
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
    return sock;
}

pcap_t *setup_handle(const char *iface) {
    char errbuf[PCAP_ERRBUF_SIZE];                      // Will store pcap errors
    pcap_t *const handle = pcap_create(iface, errbuf);  // Create a pcap capture handle (takes the interface and the error buffer)
    //pcap_set_snaplen(handle, 65536);                  // Set the max len of the packets. Good to set but the default is usually the same
    //pcap_set_timeout(handle, 100); for now redundant, because we're in immediate mode and we handle the timeouts later in the code
    pcap_set_immediate_mode(handle, 1);                 // Set the immediate mode. NEED TO LOOK AT LATER TO DECIDE WHICH IS BETTER
    pcap_setnonblock(handle, 1, NULL);
    pcap_activate(handle);                              // Applies settings & activates the capture
    return handle;
}

void setup_filter(pcap_t *handle) {
    char filter_exp[256];
    snprintf(filter_exp, sizeof(filter_exp), "tcp"); // Simple filter, good enough for now but ideally it would be more specific
    printf("[*] Using filter: %s\n", filter_exp);
    struct bpf_program fp;
    if (pcap_compile(handle, &fp, filter_exp, 0, PCAP_NETMASK_UNKNOWN) < 0 ||
        pcap_setfilter(handle, &fp) < 0) {
        fprintf(stderr, "pcap filter error\n");
        pcap_close(handle);
        exit(EXIT_FAILURE);
    }
}

void send_syn_packet(const char *local_ip, int sock, struct sockaddr_in test, int i) {
    //Printing every argument for debugging purposes
    //printf("[DEBUG] Sending SYN packet with local_ip: %s, sock: %d, test.sin_addr: %s, test.sin_port: %d, i: %d\n",
    //       local_ip, sock, inet_ntoa(test.sin_addr), ntohs(test.sin_port), i);


    //printf("[*] Sending SYN packets to ports %d-%d...\n", base_port, base_port + 9);

    const struct in_addr target_ip = test.sin_addr; // Get the target IP from the probe
    const uint16_t port = test.sin_port; // Get the port from the probe
    // Allocate only as much as needed for headers
    char packet[sizeof(struct ip) + sizeof(struct tcphdr)]; // The size is just was we need for the headers
    memset(packet, 0, sizeof(packet));
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
    const int source_port = 32768 + rand() % (65535 - 32768);   // Random source port [We should ensure it's not already in use]
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
    struct s_pseudo_header psh;                     // The pseudo header
    psh.src = iph->ip_src.s_addr;                   // Source IP address
    psh.dst = iph->ip_dst.s_addr;                   // Destination IP address
    psh.reserved = 0;                               // Always 0
    psh.protocol = IPPROTO_TCP;                     // Protocol type (TCP)
    psh.tcp_length = htons(sizeof(struct tcphdr));

    // Create a pseudo packet - Pseudo header + TCP header
    char pseudo_packet[sizeof(struct s_pseudo_header) + sizeof(struct tcphdr)];
    memset(pseudo_packet, 0, sizeof(pseudo_packet));                    // Initialization
    memcpy(pseudo_packet, &psh, sizeof(psh));                           // Copy the pseudo header into the pseudo packet
    memcpy(pseudo_packet + sizeof(psh), tcph, sizeof(struct tcphdr));   // Append the TCP header

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
    printf("[*] SYN packet sent to %s:%d\n", inet_ntoa(dest.sin_addr), ntohs(dest.sin_port));
    //sleep 100 ms
    usleep(10000);
    //
}

t_task *build_tasks(t_probe *probes, uint16_t probe_count) {
    //create the same number of tasks as probes
    t_task *tasks = malloc(sizeof(t_task) *  probe_count);
    if (!tasks) {
        perror("Failed to allocate memory for tasks");
        exit(EXIT_FAILURE);
    }
    for (uint16_t i = 0; i < probe_count; i++) {
        tasks[i].probe = probes[i];
        tasks[i].status = CREATED;
        tasks[i].result = FILTERED;
        tasks[i].function = send_syn_packet;
    }
    return tasks;
}

t_probe *build_probes(uint16_t probe_count) {
    // Create an array of probes with different scan types
    t_probe *probes = malloc(sizeof(t_probe) * probe_count);
    if (!probes) {
        perror("Failed to allocate memory for probes");
        exit(EXIT_FAILURE);
    }
    //initialize the ports and ips
    uint16_t ports[] = {80, 81, 444, 443};
    const char *ips[] = {"45.33.32.156", "45.33.32.156", "8.8.8.8", "8.8.8.8"};
    // Initialize the probes with different scan types
    for (uint16_t i = 0; i < 4; i++) {
        probes[i].scan_type = SYN;
        probes[i].addr.sin_family = AF_INET;
        probes[i].addr.sin_port = htons(ports[i]);
        probes[i].addr.sin_addr.s_addr = inet_addr(ips[i]);
    }
    for (uint16_t i = 4; i < probe_count; i++) {
        probes[i].scan_type = SYN;
        probes[i].addr.sin_family = AF_INET;
        probes[i].addr.sin_port = htons(443 + i); // Use the same ports for simplicity
        probes[i].addr.sin_addr.s_addr = inet_addr("8.8.8.8"); // Use the same IPs for simplicity
    }
    return probes;
}

void print_results(t_task *tasks, uint16_t probe_count) {
    printf("[*] Scan results:\n");
    for (uint16_t i = 0; i < probe_count; i++) {
        const char *result_str;
        switch (tasks[i].result) {
            case OPEN:
                result_str = "OPEN      ";
                break;
            case CLOSED:
                result_str = "CLOSED    ";
                break;
            case FILTERED:
                result_str = "FILTERED  ";
                break;
            default:
                result_str = "UNKNOWN   ";
        }
        printf("%s | %s %d\n", result_str, inet_ntoa(tasks[i].probe.addr.sin_addr), ntohs(tasks[i].probe.addr.sin_port));
    }
}

void listen_for_responses(pcap_t *handle, t_task *tasks, int probe_count) {
    //printf("0\n");
    // This assumes probe_count is the number of tasks

    int responses_received[probe_count];
    memset(responses_received, 0, sizeof(responses_received));
    int total_responses = 0;

    const int pcap_fd = pcap_get_selectable_fd(handle);
    time_t start = time(NULL);

    while (total_responses < probe_count) {
        fd_set read_fds;
        struct timeval timeout;

        FD_ZERO(&read_fds);
        FD_SET(pcap_fd, &read_fds);

        // Wait up to 2 seconds for responses
        const time_t elapsed = time(NULL) - start;
        const time_t remaining = 2 - elapsed;
        if (remaining <= 0) break;

        timeout.tv_sec = remaining;
        timeout.tv_usec = 0;

        //printf("1\n");
        int select_result = select(pcap_fd + 1, &read_fds, NULL, NULL, &timeout); // Wait for the pcap_fd to be ready. fd + 1 because it's checking a range
        //printf("2\n");
        if (select_result > 0 && FD_ISSET(pcap_fd, &read_fds)) { // Check if pcap_fd is ready for reading
            struct pcap_pkthdr *header;
            const u_char *data;
            //printf("3\n");
            int result = pcap_next_ex(handle, &header, &data); // Read the next packet
            //printf("4\n");
            if (result == 1) {
                struct ip *rip = (struct ip *)(data + 14); // skip Ethernet
                struct tcphdr *rtcp = (struct tcphdr *)((u_char *)rip + (rip->ip_hl << 2));

                // The ACK field in the response should be the sequence number we sent + 1
                uint32_t ack = ntohl(rtcp->th_ack);

                // Find which task this response corresponds to
                for (int i = 0; i < probe_count; ++i) {
                    // We sent SYN with th_seq = htonl(i), so expect ack == i + 1
                    if (!responses_received[i] && ack == (uint32_t)(i + 1)) {
                        if ((rtcp->th_flags & TH_SYN) && (rtcp->th_flags & TH_ACK)) {
                            //printf("[+] Port %d is OPEN (SYN+ACK)\n", ntohs(rtcp->th_sport));
                            tasks[i].result = OPEN;
                        } else if (rtcp->th_flags & TH_RST) {
                            //printf("[-] Port %d is CLOSED (RST)\n", ntohs(rtcp->th_sport));
                            tasks[i].result = CLOSED;
                        } else {
                            // Unexpected flags, treat as filtered
                            tasks[i].result = FILTERED;
                        }
                        responses_received[i] = 1;
                        total_responses++;
                        break;
                    }
                }
            } else if (result == -1) {
                fprintf(stderr, "Error reading packet: %s\n", pcap_geterr(handle));
                break;
            }
        } else if (select_result == 0) {
            // Timeout occurred
            break;
        } else if (select_result < 0) {
            perror("select() failed");
            break;
        }
    }

    // Report filtered/no response
    /*
    for (int i = 0; i < probe_count; ++i) {
        if (!responses_received[i]) {
            //printf("[?] Port %d is FILTERED or no response\n", ntohs(tasks[i].probe.addr.sin_port));
            tasks[i].result = FILTERED;
        }
    }
    */
}

void handle_packet(t_task *tasks/*, const struct pcap_pkthdr *header*/, const u_char *data) {
//    struct pcap_pkthdr *header;
//    const u_char *data;
//    int result = pcap_next_ex(handle, &header, &data); // Read the next packet

    struct ip *rip = (struct ip *)(data + 14); // skip Ethernet
    struct tcphdr *rtcp = (struct tcphdr *)((u_char *)rip + (rip->ip_hl << 2));

    // The ACK field in the response should be the sequence number we sent + 1
    uint32_t ack = ntohl(rtcp->th_ack);

    // Find which task this response corresponds to
    for (int i = 0; i < num_tasks; ++i) {
        // We sent SYN with th_seq = htonl(i), so expect ack == i + 1
        if (tasks[i].status != COMPLETED && ack == (uint32_t)(i + 1)) {
            printf("[*] Received response for task %d IN HANDLE PACKET\n", i);
            if ((rtcp->th_flags & TH_SYN) && (rtcp->th_flags & TH_ACK)) {
                printf("[+] Port %d is OPEN (SYN+ACK)\n", ntohs(rtcp->th_sport));
                tasks[i].result = OPEN;
            } else if (rtcp->th_flags & TH_RST) {
                printf("[-] Port %d is CLOSED (RST)\n", ntohs(rtcp->th_sport));
                tasks[i].result = CLOSED;
            } else {
                printf("[?] Port %d is FILTERED or no response\n", ntohs(rtcp->th_sport));
                // Unexpected flags, treat as filtered
                tasks[i].result = FILTERED;
            }
            tasks[i].status = COMPLETED;
            break;
        }
    }
}

bool check_for_answer(t_task *tasks) {
    struct pcap_pkthdr *header;
    const u_char *packet;
    int ret = pcap_next_ex(handle, &header, &packet);

    if (ret == 1) {
        handle_packet(tasks, packet);
        return true;
    } else if (ret == 0) {
        return false;
    } else {
        printf("Error receiving packet %d\n", task_index);
        return false;
    }
}

void* worker(void* arg) {
    printf("starting thread\n");
    t_task *tasks = (t_task *)arg;
    while (1) {
        if (pthread_mutex_trylock(&queue_mutex) == 0) {
            bool check_message = true;
            while (check_message) {
                check_message = check_for_answer(tasks);
            }
            pthread_mutex_unlock(&queue_mutex);
        }
        pthread_mutex_lock(&queue_mutex);
        if (task_index >= num_tasks) {
            pthread_mutex_unlock(&queue_mutex);
            break;
        }
        const int my_index = task_index;
        task_index++;
        pthread_mutex_unlock(&queue_mutex);
        tasks[my_index].function(local_ip, sock, tasks[my_index].probe.addr, my_index); // Call the function with the task ID and probe address

    }
    return NULL;
}

int main(int argc, char *argv[]) {//Remove probe_count and the other global variables later
// TEMPORARY PARSING
    if (argc != 2) {
        fprintf(stderr, "Need one arg\n");
        return 1;
    }

    int num_threads = atoi(argv[1]);
    if (num_threads < 0) {
        fprintf(stderr, "Number of threads must be a >= to 0\n");
        return 1;
    }

// PROBE CREATION
    uint16_t probe_count = 50;
    printf("[*] Building probes...\n");
    t_probe *probes = build_probes(probe_count);
    printf("[*] Created %d probes\n", scan_types_len);
    printf("[*] Building tasks from probes...\n");
    t_task *tasks = build_tasks(probes, probe_count);
    printf("[*] Created %d tasks\n", probe_count);

    // SETUP
    //char iface[IFNAMSIZ], local_ip[INET_ADDRSTRLEN];    // Retrieve the interface and ip we will use
    if (get_local_ip_and_iface(iface, local_ip) != 0) {
        perror("Could not determine local IP/interface");
        exit(EXIT_FAILURE);
    }
    printf("[*] Using interface: %s (%s)\n", iface, local_ip);

    sock = setup_socket(); // Set up the raw socket

    handle = setup_handle(iface);
    setup_filter(handle); // Needs to be improved A LOT

// SENDING THE PACKETS - NOW WITH THREADS

    pthread_t *threads = malloc((num_threads) * (sizeof(pthread_t)));
    num_tasks = probe_count; // REMOVE REDUNDANCY LATER
    for (int i = 0; i < num_threads; ++i) {
        int rc = pthread_create(&threads[i], NULL, worker, tasks);
        if (rc != 0) {
            fprintf(stderr, "Error creating thread %d: %s\n", i, strerror(rc));
            // handle cleanup or exit here
        }
    }
    worker(tasks);
    for (int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

    //worker("test main\n", "test main2\n");
    /*
    for (int i = 0; i < probe_count; ++i) {
        const struct in_addr target_ip = tasks[i].probe.addr.sin_addr; // Get the target IP from the probe
        const uint16_t port = tasks[i].probe.addr.sin_port; // Get the port from the probe
        send_syn_packet(local_ip, sock, tasks[i].probe.addr, i); // CHANGE to sending tasks, that way we can also initalize time
    }
    */
// LISTENING FOR RESPONSES
    listen_for_responses(handle, tasks, probe_count);

    print_results(tasks, probe_count); // Print the results of the scan

    pcap_close(handle);
    close(sock);
    pthread_mutex_destroy(&queue_mutex);
    return 0;
}

//sudo nmap -sS -p 80 scanme.nmap.org
//sudo ./build/bin/ft_nmap 45.33.32.156 75