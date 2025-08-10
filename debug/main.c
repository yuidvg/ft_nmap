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

static size_t build_dns_query(unsigned char *const buffer, const size_t buffer_capacity,
                              const char *const qname)
{
    // Minimal DNS query: ID, flags (RD), QDCOUNT=1
    // Name is dot-encoded labels, terminated by 0x00; QTYPE=A(1), QCLASS=IN(1)
    const size_t header_len = 12u;
    const size_t name_len = strlen(qname);

    // Compute encoded name length: sum of labels + 1 length-byte per label + trailing zero
    size_t encoded_name_len = 1u; // at least trailing zero
    for (size_t i = 0; i < name_len; ++i)
    {
        if (qname[i] == '.')
        {
            encoded_name_len += 1u; // length byte for next label
        }
        else
        {
            encoded_name_len += 1u; // character itself
        }
    }

    const size_t total_len_needed = header_len + encoded_name_len + 4u; // QTYPE + QCLASS
    if (buffer_capacity < total_len_needed)
    {
        return 0u;
    }

    memset(buffer, 0, total_len_needed);

    // Header
    buffer[0] = 0x12; // ID high
    buffer[1] = 0x34; // ID low
    buffer[2] = 0x01; // RD flag set
    buffer[3] = 0x00;
    buffer[4] = 0x00; // QDCOUNT high
    buffer[5] = 0x01; // QDCOUNT low = 1
    // ANCOUNT, NSCOUNT, ARCOUNT remain zero

    // Encode QNAME
    size_t w = header_len;
    size_t label_start = 0u;
    for (size_t i = 0; i <= name_len; ++i)
    {
        if (i == name_len || qname[i] == '.')
        {
            const size_t label_size = i - label_start;
            buffer[w++] = (unsigned char)label_size;
            for (size_t k = 0; k < label_size; ++k)
            {
                buffer[w++] = (unsigned char)qname[label_start + k];
            }
            label_start = i + 1u;
        }
    }
    buffer[w++] = 0x00; // root terminator

    // QTYPE = A(1)
    buffer[w++] = 0x00;
    buffer[w++] = 0x01;
    // QCLASS = IN(1)
    buffer[w++] = 0x00;
    buffer[w++] = 0x01;

    return w;
}

static size_t build_ntp_query(unsigned char *const buffer, const size_t buffer_capacity)
{
    const size_t ntp_len = 48u;
    if (buffer_capacity < ntp_len)
    {
        return 0u;
    }
    memset(buffer, 0, ntp_len);
    // LI=0, VN=3, Mode=3 (client) => 0x1b (widely accepted)
    buffer[0] = 0x1b;
    // Optionally set transmit timestamp to current time
    const time_t now = time(NULL);
    const uint32_t ntp_seconds = (uint32_t)(now + 2208988800u); // Unix -> NTP epoch
    buffer[40] = (unsigned char)((ntp_seconds >> 24) & 0xff);
    buffer[41] = (unsigned char)((ntp_seconds >> 16) & 0xff);
    buffer[42] = (unsigned char)((ntp_seconds >> 8) & 0xff);
    buffer[43] = (unsigned char)(ntp_seconds & 0xff);
    // fractional part left as 0
    return ntp_len;
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
    else if (datalink == DLT_LINUX_SLL)
    {
        len = 16; // Linux cooked capture v1
    }
    else if (datalink == DLT_LINUX_SLL2)
    {
        len = 20; // Linux cooked capture v2
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
    else if (port == 123)
    {
        name = "ntp";
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
    if (l2 <= 0)
    {
        return;
    }
    const unsigned char *const ip_ptr = packet + l2;
    const struct ip *const ip = (const struct ip *)ip_ptr;
    if (ip->ip_v != 4)
    {
        return;
    }
    const int ip_header_len = (int)ip->ip_hl * 4;
    if (ip_header_len < (int)sizeof(struct ip))
    {
        return;
    }
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
    const char *target_ip_str = "45.33.32.156"; // scanme.nmap.org
    const int port[30] = {
        80,  443, 53,  161, 123, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172,
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
            .sin_family = AF_INET, .sin_port = htons(port[i]), .sin_addr = {.s_addr = inet_addr(target_ip_str)}};
        // Minimal payload; add a small payload for DNS/SNMP to avoid zero-length quirks
        const int port_host = port[i];
        unsigned char payload_buf[512];
        const unsigned char one_byte = 0x00;
        const unsigned char *payload = &one_byte;
        size_t payload_size = 1;

        if (port_host == 53)
        {
            payload_size = build_dns_query(payload_buf, sizeof(payload_buf), "one.one.one.one");
            if (payload_size > 0)
            {
                payload = payload_buf;
            }
        }
        else if (port_host == 161)
        {
            // Simple non-empty payload for SNMP (not a valid PDU, just to avoid zero-length)
            payload_buf[0] = 0x30; // ASN.1 SEQUENCE start
            payload_buf[1] = 0x00; // minimal length
            payload = payload_buf;
            payload_size = 2;
        }
        else if (port_host == 123)
        {
            payload_size = build_ntp_query(payload_buf, sizeof(payload_buf));
            if (payload_size > 0)
            {
                payload = payload_buf;
            }
        }
        send_udp_datagram(sock, destination, payload, payload_size);
    }
    const uint16_t local_port = get_local_udp_port(sock);
    const uint32_t local_ip_be = inet_addr(local_ip);
    const uint32_t target_ip_be = inet_addr(target_ip_str);

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
