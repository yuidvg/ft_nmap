#define _GNU_SOURCE
#include "scanner.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

/* -------------------------------------------------------------
 * Utility — calculate an Internet checksum (RFC 1071).
 * -----------------------------------------------------------*/
static uint16_t calc_checksum(const void *const vdata, const size_t length)
{
    const uint8_t *const data        = (const uint8_t *)vdata;
    uint32_t             acc         = 0;
    const size_t         word_length = length & ~((size_t)1);

    for (size_t i = 0; i < word_length; i += 2)
    {
        const uint16_t word = (uint16_t)data[i] << 8 | (uint16_t)data[i + 1];
        acc += word;
    }

    if (length & 1U)
    {
        acc += (uint16_t)data[word_length] << 8;
    }

    /* fold */
    while (acc >> 16U)
    {
        acc = (acc & 0xffffU) + (acc >> 16U);
    }

    return (uint16_t)(~acc);
}

/* -------------------------------------------------------------
 * Build and transmit one TCP SYN packet to `port`, then listen
 * for a reply to classify the port state.
 * -----------------------------------------------------------*/
port_state_t syn_scan_port(const syn_scan_config_t config, const int port)
{
    /* single exit point per functional C guideline */
    port_state_t state = PORT_STATE_FILTERED;

    /* -----------------------------------------------------
     * 1) Resolve addresses.
     * ---------------------------------------------------*/
    struct sockaddr_in dst_addr;
    memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.sin_family = AF_INET;
    dst_addr.sin_port   = htons((uint16_t)port);

    if (inet_pton(AF_INET, config.target_ip, &dst_addr.sin_addr) != 1)
    {
        /* invalid address → treat as filtered (keeps API pure) */
        return PORT_STATE_FILTERED;
    }

    /* -----------------------------------------------------
     * 2) Create raw socket.
     * ---------------------------------------------------*/
    const int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sock == -1)
    {
        return PORT_STATE_FILTERED;
    }

    /* We build the IP header ourselves. */
    const int enable              = 1;
    const int hdrincl_res         = setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &enable, sizeof(enable));
    const bool sock_ready_success = (hdrincl_res != -1);

    if (!sock_ready_success)
    {
        close(sock);
        return PORT_STATE_FILTERED;
    }

    /* -----------------------------------------------------
     * 3) Determine our source address (best-effort).
     * ---------------------------------------------------*/
    /* Cheap trick: use a UDP socket connect to the target to let
     * the kernel choose an appropriate outgoing interface. */
    struct sockaddr_in tmp_addr;
    memset(&tmp_addr, 0, sizeof(tmp_addr));
    tmp_addr.sin_family = AF_INET;
    tmp_addr.sin_addr   = dst_addr.sin_addr;
    tmp_addr.sin_port   = htons(53); /* arbitrary */

    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct in_addr src_addr;
    src_addr.s_addr = INADDR_ANY;

    if (udp_sock != -1)
    {
        if (connect(udp_sock, (struct sockaddr *)&tmp_addr, sizeof(tmp_addr)) == 0)
        {
            struct sockaddr_in name;
            socklen_t          name_len = sizeof(name);
            if (getsockname(udp_sock, (struct sockaddr *)&name, &name_len) == 0)
            {
                src_addr = name.sin_addr;
            }
        }
        close(udp_sock);
    }

    if (src_addr.s_addr == INADDR_ANY)
    {
        /* Could not figure source IP; let the user know by treating as filtered */
        close(sock);
        return PORT_STATE_FILTERED;
    }

    /* -----------------------------------------------------
     * 4) Build the packet (IP + TCP).
     * ---------------------------------------------------*/
    const uint16_t src_port = (uint16_t)(1024 + (rand() % (65535 - 1024)));

    uint8_t packet[sizeof(struct iphdr) + sizeof(struct tcphdr)];
    memset(packet, 0, sizeof(packet));

    struct iphdr *const ip  = (struct iphdr *)packet;
    struct tcphdr *const tcp = (struct tcphdr *)(packet + sizeof(struct iphdr));

    /* IP header */
    ip->ihl      = 5; /* 20 bytes */
    ip->version  = 4;
    ip->tos      = 0;
    ip->tot_len  = htons((uint16_t)sizeof(packet));
    ip->id       = htons((uint16_t)rand());
    ip->frag_off = 0;
    ip->ttl      = 64;
    ip->protocol = IPPROTO_TCP;
    ip->check    = 0;
    ip->saddr    = src_addr.s_addr;
    ip->daddr    = dst_addr.sin_addr.s_addr;
    ip->check    = calc_checksum(ip, sizeof(struct iphdr));

    /* TCP header */
    tcp->source  = htons(src_port);
    tcp->dest    = htons((uint16_t)port);
    tcp->seq     = htonl(0);
    tcp->ack_seq = 0;
    tcp->doff    = 5; /* 20 bytes */
    tcp->syn     = 1;
    tcp->window  = htons(65535);
    tcp->check   = 0;
    tcp->urg_ptr = 0;

    /* TCP checksum incl. pseudo header */
    struct
    {
        uint32_t src;
        uint32_t dst;
        uint8_t  placeholder;
        uint8_t  protocol;
        uint16_t tcp_length;
    } pseudo_header;

    pseudo_header.src        = ip->saddr;
    pseudo_header.dst        = ip->daddr;
    pseudo_header.placeholder = 0;
    pseudo_header.protocol   = IPPROTO_TCP;
    pseudo_header.tcp_length = htons((uint16_t)sizeof(struct tcphdr));

    uint8_t pseudo_packet[sizeof(pseudo_header) + sizeof(struct tcphdr)];
    memcpy(pseudo_packet, &pseudo_header, sizeof(pseudo_header));
    memcpy(pseudo_packet + sizeof(pseudo_header), tcp, sizeof(struct tcphdr));

    tcp->check = calc_checksum(pseudo_packet, sizeof(pseudo_packet));

    /* -----------------------------------------------------
     * 5) Transmit.
     * ---------------------------------------------------*/
    const ssize_t bytes_sent = sendto(sock,
                                      packet,
                                      sizeof(packet),
                                      0,
                                      (struct sockaddr *)&dst_addr,
                                      sizeof(dst_addr));

    if (bytes_sent != (ssize_t)sizeof(packet))
    {
        close(sock);
        return PORT_STATE_FILTERED;
    }

    /* -----------------------------------------------------
     * 6) Wait for response (select → recvfrom).
     * ---------------------------------------------------*/
    fd_set      read_set;
    FD_ZERO(&read_set);
    FD_SET(sock, &read_set);

    struct timeval tv;
    tv.tv_sec  = config.timeout_ms / 1000;
    tv.tv_usec = (config.timeout_ms % 1000) * 1000;

    const int sel_res = select(sock + 1, &read_set, NULL, NULL, &tv);

    if (sel_res > 0 && FD_ISSET(sock, &read_set))
    {
        uint8_t  recv_buf[4096];
        ssize_t  recv_len;
        struct sockaddr_in reply_addr;
        socklen_t          reply_len = sizeof(reply_addr);

        recv_len = recvfrom(sock, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&reply_addr, &reply_len);

        if (recv_len >= (ssize_t)(sizeof(struct iphdr) + sizeof(struct tcphdr)))
        {
            struct iphdr *const rip  = (struct iphdr *)recv_buf;
            const size_t rip_hdr_len = (size_t)rip->ihl * 4U;
            struct tcphdr *const rtcp = (struct tcphdr *)(recv_buf + rip_hdr_len);

            const bool src_match = (rip->saddr == ip->daddr);
            const bool dst_match = (rip->daddr == ip->saddr);
            const bool port_match = (ntohs(rtcp->source) == (uint16_t)port) && (ntohs(rtcp->dest) == src_port);

            if (src_match && dst_match && port_match)
            {
                if (rtcp->syn && rtcp->ack)
                {
                    state = PORT_STATE_OPEN;
                }
                else if (rtcp->rst)
                {
                    state = PORT_STATE_CLOSED;
                }
            }
        }
    }
    else
    {
        state = PORT_STATE_FILTERED;
    }

    close(sock);
    return state;
}
