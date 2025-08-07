#include "internal.h"

const char *scan_type2str(const scan_type_t scan_type)
{
    switch (scan_type)
    {
    case SCAN_TYPE_SYN:
        return "SYN";
    case SCAN_TYPE_NULL:
        return "NULL";
    case SCAN_TYPE_FIN:
        return "FIN";
    case SCAN_TYPE_XMAS:
        return "XMAS";
    case SCAN_TYPE_ACK:
        return "ACK";
    case SCAN_TYPE_UDP:
        return "UDP";
    case SCAN_TYPE_ALL:
        return "ALL";
    default:
        return "UNKNOWN";
    }
}

void print_results(const t_task *tasks, const uint16_t num_tasks) {
    printf("[*] Scan results:\n");
    for (uint16_t i = 0; i < num_tasks; i++) {
        const char *result_str;
        switch (tasks[i].result) {
            case OPEN:
                result_str = "OPEN      ";
                break;
            case CLOSED:
                result_str = "CLOSED    ";
                break;
            case FILTERED:
                result_str = "FILTERED  ";
                break;
            default:
                result_str = "ERROR     ";
        }
        printf("%s | %s %d\n", result_str, inet_ntoa(tasks[i].probe.address.sin_addr), ntohs(tasks[i].probe.address.sin_port));
    }
}
