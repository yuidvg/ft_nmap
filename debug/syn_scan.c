#include <pcap.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/if_ether.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/*───────────────────────────────────────────────────────────────────────────*/
/*  Utility Types                                                           */
/*───────────────────────────────────────────────────────────────────────────*/

typedef struct
{
    const bool succeeded;
    const int  result;     /* file descriptor or negative errno */
} Result_fd;

/*───────────────────────────────────────────────────────────────────────────*/
/*  Checksum Helpers                                                        */
/*───────────────────────────────────────────────────────────────────────────*/

static uint16_t compute_checksum(const uint16_t *const data, const size_t len_bytes)
{
    uint32_t      sum        = 0U;
    const size_t  len_words  = len_bytes / 2U;
    for (size_t i = 0U; i < len_words; ++i)
    {
        sum += (uint32_t)data[i];
    }
    if (len_bytes % 2U == 1U)
    {
        sum += ((const uint8_t *)data)[len_bytes - 1U] << 8U;
    }

    while (sum >> 16U)
    {
        sum = (sum & 0xFFFFU) + (sum >> 16U);
    }
    return (uint16_t)(~sum);
}

static uint16_t compute_ip_checksum(struct ip *const ip_header)
{
    ip_header->ip_sum = 0U;
    return compute_checksum((const uint16_t *)ip_header, (size_t)ip_header->ip_hl * 4U);
}

static uint16_t compute_tcp_checksum(const struct ip  *const ip_header,
                                     struct tcphdr    *const tcp_header,
                                     const size_t             tcp_len)
{
    /* Pseudo-header */
    struct
    {
        uint32_t src_addr;
        uint32_t dst_addr;
        uint8_t  zero;
        uint8_t  protocol;
        uint16_t tcp_length;
    } pseudo_header;

    pseudo_header.src_addr  = ip_header->ip_src.s_addr;
    pseudo_header.dst_addr  = ip_header->ip_dst.s_addr;
    pseudo_header.zero      = 0U;
    pseudo_header.protocol  = IPPROTO_TCP;
    pseudo_header.tcp_length = htons((uint16_t)tcp_len);

    const size_t total_len = sizeof(pseudo_header) + tcp_len;
    /* Allocate contiguous buffer: pseudo header + tcp segment */
    uint8_t *const buf = (uint8_t *)calloc(1U, total_len);
    if (buf == NULL)
    {
        return 0U;
    }

    memcpy(buf, &pseudo_header, sizeof(pseudo_header));
    memcpy(buf + sizeof(pseudo_header), tcp_header, tcp_len);

    const uint16_t checksum = compute_checksum((const uint16_t *)buf, total_len);
    free(buf);
    return checksum;
}

/*───────────────────────────────────────────────────────────────────────────*/
/*  System Call Wrappers                                                    */
/*───────────────────────────────────────────────────────────────────────────*/

static Result_fd open_raw_socket(void)
{
    const int fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    return (Result_fd){ .succeeded = (fd != -1), .result = (fd != -1) ? fd : -errno };
}

static bool set_ip_hdr_incl(const int fd)
{
    const int on = 1;
    const int rc = setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on));
    return rc == 0;
}

/*───────────────────────────────────────────────────────────────────────────*/
/*  Packet Crafting                                                         */
/*───────────────────────────────────────────────────────────────────────────*/

static size_t build_syn_packet(uint8_t                *const packet_buf,
                               const struct in_addr    src_ip,
                               const struct in_addr    dst_ip,
                               const uint16_t          src_port,
                               const uint16_t          dst_port)
{
    struct ip     *const ip_header  = (struct ip *)packet_buf;
    struct tcphdr *const tcp_header = (struct tcphdr *)(packet_buf + sizeof(struct ip));

    /* IP header */
    ip_header->ip_hl   = 5;                    /* header length */
    ip_header->ip_v    = 4;                    /* IPv4          */
    ip_header->ip_tos  = 0;
    ip_header->ip_len  = htons((uint16_t)(sizeof(struct ip) + sizeof(struct tcphdr)));
    ip_header->ip_id   = htons((uint16_t)arc4random());
    ip_header->ip_off  = 0;
    ip_header->ip_ttl  = 64;
    ip_header->ip_p    = IPPROTO_TCP;
    ip_header->ip_src  = src_ip;
    ip_header->ip_dst  = dst_ip;
    ip_header->ip_sum  = compute_ip_checksum(ip_header);

    /* TCP header */
    tcp_header->source = htons(src_port);
    tcp_header->dest   = htons(dst_port);
    tcp_header->seq    = htonl(0xdeadbeefUL);
    tcp_header->ack_seq = 0U;
    tcp_header->doff   = 5;     /* 5 × 4 = 20 bytes */
    tcp_header->syn    = 1U;
    tcp_header->window = htons(65535);
    tcp_header->check  = 0U;    /* calculate later */
    tcp_header->urg_ptr = 0U;

    tcp_header->check = compute_tcp_checksum(ip_header,
                                             tcp_header,
                                             sizeof(struct tcphdr));

    return sizeof(struct ip) + sizeof(struct tcphdr);
}

/*───────────────────────────────────────────────────────────────────────────*/
/*  Pcap Helpers                                                            */
/*───────────────────────────────────────────────────────────────────────────*/

static pcap_t * open_pcap_live(const char *const dev, char *const errbuf)
{
    /* 96 bytes snapshot is enough for IP+TCP headers */
    return pcap_open_live(dev, 96, 0, 1000, errbuf);
}

/*───────────────────────────────────────────────────────────────────────────*/
/*  Main                                                                    */
/*───────────────────────────────────────────────────────────────────────────*/

int main(const int argc, char *const argv[])
{
    int exit_status = EXIT_FAILURE;   /* single return pattern */

    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s <interface> <target_ip> <target_port>\n", argv[0]);
    }
    else
    {
        const char *const interface  = argv[1];
        const char *const target_str = argv[2];
        const uint16_t   target_port = (uint16_t)atoi(argv[3]);

        struct in_addr dst_ip = {0};
        const int      inet_ok = inet_aton(target_str, &dst_ip);
        if (inet_ok == 0)
        {
            fprintf(stderr, "Invalid target IP %s\n", target_str);
        }
        else
        {
            /* Determine our own IP address on the given interface */
            int             sock_tmp = socket(AF_INET, SOCK_DGRAM, 0);
            struct ifreq    ifr;
            memset(&ifr, 0, sizeof(ifr));
            strncpy(ifr.ifr_name, interface, sizeof(ifr.ifr_name) - 1);
            (void)ioctl(sock_tmp, SIOCGIFADDR, &ifr);
            close(sock_tmp);
            const struct sockaddr_in *const addr_in = (struct sockaddr_in *)&ifr.ifr_addr;
            const struct in_addr src_ip = addr_in->sin_addr;

            /* Raw socket setup */
            const Result_fd raw_fd = open_raw_socket();
            if (!raw_fd.succeeded || !set_ip_hdr_incl(raw_fd.result))
            {
                fprintf(stderr, "Failed to create raw socket (%s)\n", strerror(errno));
            }
            else
            {
                /* Build SYN Packet */
                uint8_t packet[sizeof(struct ip) + sizeof(struct tcphdr)] = {0};
                const uint16_t src_port = (uint16_t) (10000U + (arc4random() % 50000U));
                const size_t packet_len = build_syn_packet(packet, src_ip, dst_ip, src_port, target_port);

                /* Destination */
                struct sockaddr_in dst = {0};
                dst.sin_family = AF_INET;
                dst.sin_addr   = dst_ip;

                if (sendto(raw_fd.result, packet, packet_len, 0,
                           (struct sockaddr *)&dst, sizeof(dst)) == -1)
                {
                    fprintf(stderr, "sendto failed (%s)\n", strerror(errno));
                }
                else
                {
                    /* Set up pcap capture */
                    char errbuf[PCAP_ERRBUF_SIZE] = {0};
                    pcap_t *const handle = open_pcap_live(interface, errbuf);
                    if (handle == NULL)
                    {
                        fprintf(stderr, "pcap_open_live: %s\n", errbuf);
                    }
                    else
                    {
                        /* Berkeley Packet Filter: looking for TCP from target IP to us, src port target_port, flags SYN|ACK or RST */
                        char filter_exp[128] = {0};
                        snprintf(filter_exp, sizeof(filter_exp),
                                 "tcp and src host %s and dst port %u",
                                 target_str, src_port);
                        struct bpf_program fp;
                        if (pcap_compile(handle, &fp, filter_exp, 0, PCAP_NETMASK_UNKNOWN) == -1 ||
                            pcap_setfilter(handle, &fp) == -1)
                        {
                            fprintf(stderr, "pcap filter compile/set failed: %s\n", pcap_geterr(handle));
                        }
                        else
                        {
                            /* Capture with 3-second timeout */
                            const struct pcap_pkthdr *header;
                            const uint8_t *data;
                            int ret = 0;
                            const time_t start = time(NULL);
                            while (((ret = pcap_next_ex(handle, &header, &data)) >= 0) &&
                                   (time(NULL) - start < 3))
                            {
                                if (ret == 0) { continue; } /* timeout */
                                const struct ip *ip_resp  = (const struct ip *)(data + sizeof(struct ether_header));
                                const struct tcphdr *tcp_resp = (const struct tcphdr *)((const uint8_t *)ip_resp + (ip_resp->ip_hl * 4));
                                const bool is_syn_ack = (tcp_resp->syn == 1U) && (tcp_resp->ack == 1U);
                                const bool is_rst     = (tcp_resp->rst == 1U);
                                if (is_syn_ack)
                                {
                                    printf("Port %u is OPEN (SYN-ACK)\n", target_port);
                                    exit_status = EXIT_SUCCESS;
                                    break;
                                }
                                else if (is_rst)
                                {
                                    printf("Port %u is CLOSED (RST)\n", target_port);
                                    exit_status = EXIT_SUCCESS;
                                    break;
                                }
                            }
                            if (exit_status != EXIT_SUCCESS)
                            {
                                printf("Port %u is FILTERED (no response)\n", target_port);
                                exit_status = EXIT_SUCCESS;
                            }
                        }
                        pcap_close(handle);
                    }
                }
                close(raw_fd.result);
            }
        }
    }

    return exit_status;
}