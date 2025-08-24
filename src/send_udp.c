# include "internal.h"

uint8_t get_udp_payload_size(uint16_t port) {
    if (port == 53 || port == 123 || port == 161)
        return 48;
    return 0;
}

void fill_udp_payload(unsigned char *payload, uint8_t size, uint16_t port) {
    memset(payload, 0, size);

    if (port == 53) // DNS
    {
        // Simple DNS query for "example.com"
        const unsigned char dns_query[] = {
            0x12, 0x34,                         // Arbitrary Transaction ID
            0x01, 0x00, 0x00, 0x01,             // Standard query, One question
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // No answers, No authority, No additional
            0x06, 'g', 'o', 'o', 'g', 'l', 'e', // Our query. The first byte is the length, google is an arbitrary choice
            0x03, 'c', 'o', 'm', 0x00,
            0x00, 0x01, 0x00, 0x01              // Type A Class IN
        };
        memcpy(payload, dns_query, sizeof(dns_query) < size ? sizeof(dns_query) : size);
    }
    else if (port == 161) // SNMP
    {
        // Used to get device info
        const unsigned char snmp_query[] = {
            0x30, 0x26,                                 // Sequence
            0x02, 0x01, 0x00,                           // Version: SNMPv1
            0x04, 0x06, 'p', 'u', 'b', 'l', 'i', 'c',   // Community: public
            0xa0, 0x19,                                 // GetRequest PDU
            0x02, 0x04, 0x70, 0x91, 0x5f, 0xc4,         // Request ID
            0x02, 0x01, 0x00, 0x02, 0x01, 0x00,         // Error status and index
            0x30, 0x0b, 0x30, 0x09,                     // Variable bindings         
            0x06, 0x05, 0x2b, 0x06, 0x01, 0x02, 0x01,   // Object Identifiers
            0x05, 0x00                                  // Null value
        };
        memcpy(payload, snmp_query, sizeof(snmp_query) < size ? sizeof(snmp_query) : size);
    }
    else if (port == 123) // NTP
    {
        payload[0] = 0x1b; // The first byte is Version Number = 3, Mode = 3 (client)
    }
}

void fill_udp_header(struct udphdr *udph, const struct ip *iph, const struct sockaddr_in *dest, uint8_t udp_payload_size) {
    udph->uh_sport = htons((DEFAULT_PORT + SCAN_TYPE_UDP) + (ntohs(dest->sin_port) % (DEFAULT_PORT - SCAN_TYPE_UDP)));
    udph->uh_dport = dest->sin_port;
    udph->uh_ulen = htons(sizeof(struct udphdr) + udp_payload_size);
    
    // Calculate UDP checksum using pseudo header
    char pseudo_packet[sizeof(t_pseudo_header) + sizeof(struct udphdr) + udp_payload_size];
    fill_pseudo_packet(pseudo_packet, iph, udph, sizeof(struct udphdr) + udp_payload_size, IPPROTO_UDP);
    udph->uh_sum = 0;
    udph->uh_sum = checksum((unsigned short *)pseudo_packet, sizeof(t_pseudo_header) + sizeof(struct udphdr) + udp_payload_size);
    if (udph->uh_sum == 0) udph->uh_sum = 0xffff; // For UDP, a checksum of zero is transmitted as all ones because zero means no checksum
}

void send_udp_packet(const char *local_ip, const int sock, const struct sockaddr_in dest) {
    uint8_t udp_payload_size = get_udp_payload_size(ntohs(dest.sin_port));
    char packet[sizeof(struct ip) + sizeof(struct udphdr) + udp_payload_size];
    memset(packet, 0, sizeof(packet));

    struct ip *iph = (struct ip *)packet;
    struct udphdr *udph = (struct udphdr *)(packet + sizeof(struct ip));
    unsigned char *payload = (unsigned char *)(packet + sizeof(struct ip) + sizeof(struct udphdr));

    fill_udp_payload(payload, udp_payload_size, ntohs(dest.sin_port));
    fill_ip_header(iph, local_ip, &dest, IPPROTO_UDP);
    fill_udp_header(udph, iph, &dest, udp_payload_size);

    // Send the packet
    if (sendto(sock, packet, sizeof(packet), 0, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
        perror("UDP sendto failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
}