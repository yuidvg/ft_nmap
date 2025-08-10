#include "internal.h"
#include "type.h"

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
                               .uh_sport = htons(0),                                           // set source port
                               .uh_dport = htons(destination.sin_port),                        // set destination port
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


