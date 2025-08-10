#include "type.h"

static unsigned short checksum(unsigned short *ptr, int nbytes)
{
    long sum = 0;
    unsigned short oddbyte;
    while (nbytes > 1)
    {
        sum += *ptr++;
        nbytes -= 2;
    }
    if (nbytes == 1)
    {
        oddbyte = 0;
        *((unsigned char *)&oddbyte) = *(unsigned char *)ptr;
        sum += oddbyte;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

static size_t payload_len [[maybe_unused]] (udp_packet_t packet)
{
    return (size_t)ntohs(packet.udp_header.uh_ulen);
}

static size_t packet_len(udp_packet_t packet)
{
    return (size_t)ntohs(packet.ip_header.ip_len);
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
