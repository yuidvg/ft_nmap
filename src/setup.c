# include <../include/all.h>

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
    pcap_setnonblock(handle, 1, errbuf);                // Set the non-blocking mode (so we can handle the packets as they come in)
    pcap_activate(handle);                              // Applies settings & activates the capture
    return handle;
}

void setup_filter(pcap_t *handle, char *local_ip) {
    char filter_exp[256];
    //set tcp while excluding the local ip
    snprintf(filter_exp, sizeof(filter_exp), "(tcp or udp or icmp) and not src host %s", local_ip);
    printf("[*] Using filter: %s\n", filter_exp);
    struct bpf_program fp;
    if (pcap_compile(handle, &fp, filter_exp, 0, PCAP_NETMASK_UNKNOWN) < 0 ||
        pcap_setfilter(handle, &fp) < 0) {
        fprintf(stderr, "pcap filter error\n");
        pcap_close(handle);
        exit(EXIT_FAILURE);
    }
}