#include "internal.h"

void parse_syn(t_task *task, const struct tcphdr *rtcp) {
    if ((rtcp->th_flags & TH_SYN) && (rtcp->th_flags & TH_ACK)) {
        if (DEBUG) printf("[+] Port %d is OPEN (SYN+ACK)\n", ntohs(rtcp->th_sport));
        task->result = OPEN;
    } else if (rtcp->th_flags & TH_RST) {
        if (DEBUG) printf("[-] Port %d is CLOSED (RST)\n", ntohs(rtcp->th_sport));
        task->result = CLOSED;
    } else {
        if (DEBUG) printf("[?] Port %d is UNEXPECTED\n", ntohs(rtcp->th_sport));
        task->result = UNEXPECTED;
    }
}

void parse_ack(t_task *task, const struct tcphdr *rtcp) {
    if (rtcp->th_flags & TH_RST) {
        task->result = UNFILTERED;
    } else {
        task->result = FILTERED;
    }
}

void parse_stealth(t_task *task, const struct tcphdr *rtcp) { // NUL, FIN, XMAS
    if (rtcp->th_flags & TH_RST) {
        task->result = CLOSED;
    } else {
        task->result = OPEN_FILTERED;
    }
}

void parse_udp(t_task *task, const struct tcphdr *rtcp) {
    if (rtcp->th_flags & TH_ACK) {
        task->result = OPEN;
    } else if (rtcp->th_flags & TH_RST) {
        task->result = CLOSED;
    } else {
        task->result = FILTERED;
    }
}

int get_task_index(const t_task *tasks, const uint16_t num_tasks, const struct in_addr ip, const uint16_t port, const uint16_t host_port, const int protocol) {
    for (uint16_t i = 0; i < num_tasks; i++) {
        if (tasks[i].status == COMPLETED) continue;
        if (protocol == IPPROTO_TCP) {
            if (tasks[i].probe.address.sin_addr.s_addr == ip.s_addr && ntohs(tasks[i].probe.address.sin_port) == host_port && (int)tasks[i].probe.scan_type == port - DEFAULT_PORT) {
                if (DEBUG) printf("[*] Found task %d\n", i);
                return i;
            }
        } else {
            if (tasks[i].probe.address.sin_addr.s_addr == ip.s_addr && ntohs(tasks[i].probe.address.sin_port) == port && tasks[i].probe.scan_type == SCAN_TYPE_UDP) {
                if (DEBUG) printf("[*] Found task %d\n", i);
                return i;
            }
        }
    }
    return -1;
}

void handle_packet(t_task *tasks, const u_char *data, const uint16_t num_tasks) {
    const struct ip *rip = (struct ip *)(data + 14); // skip Ethernet

    if (rip->ip_p == IPPROTO_TCP) {
        const struct tcphdr *rtcp = (struct tcphdr *)((u_char *)rip + (rip->ip_hl << 2));

        if (DEBUG) printf("[*] Searching for task with ip %s, port %d, host_port %d, flag %d\n", inet_ntoa(rip->ip_src), ntohs(rtcp->th_dport), ntohs(rtcp->th_sport), rtcp->th_flags);
        const int i = get_task_index(tasks, num_tasks, rip->ip_src, ntohs(rtcp->th_dport), ntohs(rtcp->th_sport), rip->ip_p);
        // Find which task this response corresponds to
        if (tasks[i].status != COMPLETED && i != -1) {
            if (TIMEOUT < tasks[i].start - time(NULL)) {
                if (DEBUG) printf("[!] Task %d timed out\n", i);
                tasks[i].result = FILTERED;
                tasks[i].status = COMPLETED;
                return;
            }
            if (DEBUG) printf("[*] Received response for task %d IN HANDLE PACKET\n", i);
            switch (tasks[i].probe.scan_type) {
                case SCAN_TYPE_SYN:
                    parse_syn(&tasks[i], rtcp);
                    break;
                case SCAN_TYPE_ACK:
                    parse_ack(&tasks[i], rtcp);
                    break;
                case SCAN_TYPE_NULL:
                case SCAN_TYPE_FIN:
                case SCAN_TYPE_XMAS:
                    parse_stealth(&tasks[i], rtcp);
                    break;
                case SCAN_TYPE_UDP:
                    parse_udp(&tasks[i], rtcp);
                    break;
                default:
                    fprintf(stderr, "Unknown scan type for task %d\n", i);
            }
            tasks[i].status = COMPLETED;
            //if (DEBUG) printf("Task %d completed has service %s\n", i, service_name(ntohs(rtcp->th_sport), "tcp"));
        }
    } else if (rip->ip_p == IPPROTO_ICMP) {
        const struct icmp *ricmp = (struct icmp *)((u_char *)rip + (rip->ip_hl << 2));
        if (ricmp->icmp_type == ICMP_UNREACH && ricmp->icmp_code == ICMP_UNREACH_PORT) {
            const struct ip *orig_ip = (struct ip *)(ricmp->icmp_data);
            if (orig_ip->ip_p == IPPROTO_UDP) {
                const struct udphdr *orig_udp = (struct udphdr *)((u_char *)orig_ip + (orig_ip->ip_hl << 2));
                const int i = get_task_index(tasks, num_tasks, orig_ip->ip_dst, ntohs(orig_udp->uh_dport), ntohs(orig_udp->uh_sport), rip->ip_p);
                if (tasks[i].status != COMPLETED && i != -1) {
                    tasks[i].result = CLOSED;
                    tasks[i].status = COMPLETED;
                    if (DEBUG) printf("[ICMP] Port %d is CLOSED (ICMP Port Unreachable)\n", ntohs(orig_udp->uh_dport));
                }
            }
        }
    }
}

bool check_for_answer(t_task *tasks, pcap_t *handle, const uint16_t num_tasks) {
    struct pcap_pkthdr *header;
    const u_char *packet;
    const int ret = pcap_next_ex(handle, &header, &packet);

    if (ret == 1)
    {
        handle_packet(tasks, packet, num_tasks);
        return true;
    }
    else
    {
        return false;
    }
}
