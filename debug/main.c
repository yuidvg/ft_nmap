#define _GNU_SOURCE

#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <time.h>
#include "../src/type.h"

// Send a UDP datagram using SOCK_DGRAM. Kernel builds headers and checksum.
static void send_udp_datagram(const int sock, const struct sockaddr_in destination,
                              const unsigned char *const payload, const size_t payload_len)
{
    (void)sendto(sock, payload, payload_len, 0, (const struct sockaddr *)&destination, sizeof(destination));
}

static pcap_t *setup_handle(const char *iface)
{
    char errbuf[PCAP_ERRBUF_SIZE]; // Will store pcap errors
    pcap_t *const handle =
        pcap_create(iface, errbuf); // Create a pcap capture handle (takes the interface and the error buffer)
    // pcap_set_snaplen(handle, 65536);                  // Set the max len of the packets. Good to set but the default
    // is usually the same pcap_set_timeout(handle, 100); for now redundant, because we're in immediate mode and we
    // handle the timeouts later in the code
    pcap_set_immediate_mode(handle, 1);  // Set the immediate mode. NEED TO LOOK AT LATER TO DECIDE WHICH IS BETTER
    pcap_setnonblock(handle, 1, errbuf); // Set the non-blocking mode (so we can handle the packets as they come in)
    pcap_activate(handle);               // Applies settings & activates the capture
    return handle;
}

static void setup_filter(pcap_t *handle)
{
    char filter_exp[256];
    snprintf(
        filter_exp, sizeof(filter_exp),
        "icmp or icmp6 or tcp or udp or sctp"); // Simple filter,
                                                                                                   // good enough for
                                                                                                   // now but ideally it
                                                                                                   // would be more
                                                                                                   // specific
    printf("[*] Using filter: %s\n", filter_exp);
    struct bpf_program fp;
    if (pcap_compile(handle, &fp, filter_exp, 0, PCAP_NETMASK_UNKNOWN) < 0 || pcap_setfilter(handle, &fp) < 0)
    {
        fprintf(stderr, "pcap filter error\n");
        pcap_close(handle);
        exit(EXIT_FAILURE);
    }
    pcap_freecode(&fp);
}

static uint16_t get_local_udp_port(const int sock)
{
    struct sockaddr_in addr;
    socklen_t addrlen = (socklen_t)sizeof(addr);
    memset(&addr, 0, sizeof(addr));
    int ok = getsockname(sock, (struct sockaddr *)&addr, &addrlen);
    uint16_t port = 0;
    if (ok == 0)
    {
        port = ntohs(addr.sin_port);
    }
    return port;
}

static int ethernet_header_length(const int datalink)
{
    int len = 0;
    if (datalink == DLT_EN10MB)
    {
        len = 14;
    }
    return len;
}

static int find_port_index(const int *const ports, const size_t num_ports, const int port)
{
    size_t i = 0;
    int idx = -1;
    while (i < num_ports && idx == -1)
    {
        if (ports[i] == port)
        {
            idx = (int)i;
        }
        i += 1;
    }
    return idx;
}

static const char *udp_service_name(const int port)
{
    const char *name = "unknown";
    if (port == 53)
    {
        name = "domain";
    }
    else if (port == 161)
    {
        name = "snmp";
    }
    else if (port == 80)
    {
        name = "http";
    }
    else if (port == 443)
    {
        name = "https";
    }
    return name;
}

static void classify_packet(const u_char *const packet,
                            const int datalink,
                            const uint32_t local_ip_be,
                            const uint16_t local_port,
                            const uint32_t target_ip_be,
                            const int *const ports,
                            const size_t num_ports,
                            t_result *const results)
{
    const int l2 = ethernet_header_length(datalink);
    const unsigned char *const ip_ptr = packet + l2;
    const struct ip *const ip = (const struct ip *)ip_ptr;
    const int ip_header_len = (int)ip->ip_hl * 4;
    const unsigned char *const l4_ptr = ip_ptr + ip_header_len;

    if (ip->ip_p == IPPROTO_UDP)
    {
        const struct udphdr *const udp = (const struct udphdr *)l4_ptr;
        const uint32_t src_ip_be = ((const struct in_addr *)&ip->ip_src)->s_addr;
        const uint16_t src_port = ntohs(udp->uh_sport);
        const uint16_t dst_port = ntohs(udp->uh_dport);

        if (src_ip_be == target_ip_be && dst_port == local_port)
        {
            const int idx = find_port_index(ports, num_ports, (int)src_port);
            if (idx >= 0)
            {
                results[idx] = OPEN;
            }
        }
    }
    else if (ip->ip_p == IPPROTO_ICMP)
    {
        const struct icmphdr *const icmp = (const struct icmphdr *)l4_ptr;
        if (icmp->type == 3) // Destination Unreachable
        {
            const unsigned char *const inner_ip_ptr = l4_ptr + sizeof(struct icmphdr);
            const struct ip *const inner_ip = (const struct ip *)inner_ip_ptr;
            if (inner_ip->ip_p == IPPROTO_UDP)
            {
                const int inner_ip_hl = (int)inner_ip->ip_hl * 4;
                const struct udphdr *const inner_udp = (const struct udphdr *)(inner_ip_ptr + inner_ip_hl);
                const uint16_t inner_dst_port = ntohs(inner_udp->uh_dport);
                const uint16_t inner_src_port = ntohs(inner_udp->uh_sport);
                const uint32_t inner_dst_ip_be = ((const struct in_addr *)&inner_ip->ip_dst)->s_addr;
                const uint32_t inner_src_ip_be = ((const struct in_addr *)&inner_ip->ip_src)->s_addr;

                if (inner_src_ip_be == local_ip_be && inner_dst_ip_be == target_ip_be && inner_src_port == local_port)
                {
                    const int idx = find_port_index(ports, num_ports, (int)inner_dst_port);
                    if (idx >= 0)
                    {
                        if (icmp->code == 3)
                        {
                            results[idx] = CLOSED;
                        }
                        else if (icmp->code == 1 || icmp->code == 2 || icmp->code == 9 || icmp->code == 10 ||
                                 icmp->code == 13)
                        {
                            results[idx] = FILTERED;
                        }
                    }
                }
            }
        }
    }
}

static void print_summary(const int *const ports, const size_t num_ports, const t_result *const results)
{
    size_t unknown = 0u;
    size_t i = 0u;
    while (i < num_ports)
    {
        if (results[i] != OPEN && results[i] != CLOSED && results[i] != FILTERED)
        {
            unknown += 1u;
        }
        i += 1u;
    }

    if (unknown > 0u)
    {
        printf("Not shown: %zu open|filtered udp ports (no-response)\n", unknown);
    }

    printf("PORT\tSTATE\tSERVICE\n");
    i = 0u;
    while (i < num_ports)
    {
        const int port = ports[i];
        const char *state = NULL;
        if (results[i] == OPEN)
        {
            state = "open";
        }
        else if (results[i] == CLOSED)
        {
            state = "closed";
        }
        else if (results[i] == FILTERED)
        {
            state = "filtered";
        }

        if (state != NULL)
        {
            printf("%d/udp\t%s\t%s\n", port, state, udp_service_name(port));
        }

        i += 1u;
    }
}

int main()
{
    const char *local_ip = "10.0.2.15";
    const int port[30] = {
        80,  443, 53,  161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172,
        173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187,
    };
    const int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return 1;
    }

    // Bind to local IP, ephemeral port
    struct sockaddr_in bind_addr = {0};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = inet_addr(local_ip);
    bind_addr.sin_port = htons(0);
    if (bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0)
    {
        perror("bind");
        close(sock);
        return 1;
    }
    pcap_t *handle = setup_handle("eth0");
    setup_filter(handle);

    // Track results per port
    t_result results[30];
    for (int r = 0; r < 30; ++r)
    {
        results[r] = UNEXPECTED; // unknown initially
    }
    for (int i = 0; i < 30; i++)
    {
        struct sockaddr_in destination = {
            .sin_family = AF_INET, .sin_port = htons(port[i]), .sin_addr = {.s_addr = inet_addr("1.1.1.1")}};
        // Minimal payload; add a small payload for DNS/SNMP to avoid zero-length quirks
        const int port_host = port[i];
        const unsigned char dns_like_payload[24] = {0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10};
        const unsigned char one_byte = 0x00;
        const unsigned char *payload = &one_byte;
        size_t payload_size = 1;
        if (port_host == 53 || port_host == 161)
        {
            payload = dns_like_payload;
            payload_size = sizeof(dns_like_payload);
        }
        send_udp_datagram(sock, destination, payload, payload_size);
    }
    const uint16_t local_port = get_local_udp_port(sock);
    const uint32_t local_ip_be = inet_addr(local_ip);
    const uint32_t target_ip_be = inet_addr("1.1.1.1");

    const time_t start_time = time(NULL);
    while (1)
    {
        struct pcap_pkthdr *header;
        const u_char *packet;
        const int ret = pcap_next_ex(handle, &header, &packet);
        if (ret == 1)
        {
            classify_packet(packet, pcap_datalink(handle), local_ip_be, local_port, target_ip_be, port,
                            (size_t)30, results);
        }
        else
        {
            usleep(100000);
        }

        // Stop after timeout or when all ports classified OPEN/CLOSED/FILTERED
        size_t remaining = 0u;
        for (int k = 0; k < 30; ++k)
        {
            if (results[k] != OPEN && results[k] != CLOSED && results[k] != FILTERED)
            {
                remaining += 1u;
            }
        }
        const time_t now = time(NULL);
        if (remaining == 0u || (now - start_time) >= 3)
        {
            break;
        }
    }
    print_summary(port, (size_t)30, results);
    pcap_close(handle);
    close(sock);
    return 0;
}
