# include <../include/all.h>

char *print_scan_type(const t_scan_types scan_type) {
    switch (scan_type) {
        case SYN:
            return "SYN";
        case NUL:
            return "NUL";
        case ACK:
            return "ACK";
        case FIN:
            return "FIN";
        case XMAS:
            return "XMAS";
        case UDP:
            return "UDP";
        default:
            return "UNKNOWN";
    }
}
            

void print_results(t_task *tasks, const uint16_t num_tasks) {
    printf("[*] Scan results:\n");
    for (uint16_t i = 0; i < num_tasks; i++) {
        const char *result_str;
        if (tasks[i].result == FILTERED && tasks[i].probe.scan_type != SYN && tasks[i].probe.scan_type != ACK) { //tmp, can be done more cleanly
            tasks[i].result = OPEN_FILTERED;
        }
        switch (tasks[i].result) {
            case OPEN:
                result_str = "OPEN          ";
                break;
            case CLOSED:
                result_str = "CLOSED        ";
                break;
            case FILTERED:
                result_str = "FILTERED      ";
                break;
            case OPEN_FILTERED:
                result_str = "OPEN|FILTERED ";
                break;
            case UNFILTERED:
                result_str = "UNFILTERED    ";
                break;
            default:
                result_str = "ERROR         ";
        }
        printf("%s | %s %s %d\n", result_str, print_scan_type(tasks[i].probe.scan_type), inet_ntoa(tasks[i].probe.addr.sin_addr), ntohs(tasks[i].probe.addr.sin_port));
    }
}
