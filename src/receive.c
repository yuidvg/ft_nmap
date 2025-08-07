# include <../include/all.h>

void handle_packet(t_task *tasks, const u_char *data, const uint16_t num_tasks) {
    //printf("[*] Handling packet...\n");
    const struct ip *rip = (struct ip *)(data + 14); // skip Ethernet
    const struct tcphdr *rtcp = (struct tcphdr *)((u_char *)rip + (rip->ip_hl << 2));

    // The ACK field in the response should be the sequence number we sent + 1
    const uint32_t ack = ntohl(rtcp->th_ack);

    // Find which task this response corresponds to
    for (uint16_t i = 0; i < num_tasks; ++i) {
        // We sent SYN with th_seq = htonl(i), so expect ack == i + 1
        if (tasks[i].status != COMPLETED && ack == (uint32_t)(i + 1)) {
            if (TIMEOUT < tasks[i].start - time(NULL)) {
                printf("[!] Task %d timed out\n", i);
                tasks[i].result = FILTERED;
                tasks[i].status = COMPLETED;
                return;
            }
            if (DEBUG) printf("[*] Received response for task %d IN HANDLE PACKET\n", i);
            if ((rtcp->th_flags & TH_SYN) && (rtcp->th_flags & TH_ACK)) {
                if (DEBUG) printf("[+] Port %d is OPEN (SYN+ACK)\n", ntohs(rtcp->th_sport));
                tasks[i].result = OPEN;
            } else if (rtcp->th_flags & TH_RST) {
                if (DEBUG) printf("[-] Port %d is CLOSED (RST)\n", ntohs(rtcp->th_sport));
                tasks[i].result = CLOSED;
            } else {
                if (DEBUG) printf("[?] Port %d is UNEXPECTED\n", ntohs(rtcp->th_sport));
                tasks[i].result = UNEXPECTED;
            }
            tasks[i].status = COMPLETED;
            break;
        }
    }
}

bool check_for_answer(t_task *tasks, pcap_t *handle, const uint16_t num_tasks) {
    struct pcap_pkthdr *header;
    const u_char *packet;
    const int ret = pcap_next_ex(handle, &header, &packet);

    if (ret == 1) {
        handle_packet(tasks, packet, num_tasks);
        return true;
    } else if (ret == 0) {
        return false;
    } else {
        return false;
    }
}