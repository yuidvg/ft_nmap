#include "internal.h"

#define OPEN_PORTS 1
#define CLOSED_PORTS 1 << 1
#define FILTERED_PORTS 1 << 2

//change to flags later
#define DISPLAY_FILTERED 0
#define TEST_DISPLAY 1

char *get_service_name(const uint16_t port, const char *protocol) {
    struct servent *se;
    se = getservbyport(htons(port), protocol); // "tcp" or "udp"
    return se ? se->s_name : "Unassigned";
}

char *scan_type2str(const scan_type_t scan_type) {
    switch (scan_type) {
    case SCAN_TYPE_SYN:
        return "SYN";
    case SCAN_TYPE_NULL:
        return "NULL";
    case SCAN_TYPE_ACK:
        return "ACK";
    case SCAN_TYPE_FIN:
        return "FIN";
    case SCAN_TYPE_XMAS:
        return "XMAS";
    case SCAN_TYPE_UDP:
        return "UDP";
    case SCAN_TYPE_ALL:
        return "ALL";
    default:
        return "UNKNOWN";
    }
}

char *scan_family2str(const scan_type_t scan_type) {
    switch (scan_type) {
    case SCAN_TYPE_SYN:
    case SCAN_TYPE_NULL:
    case SCAN_TYPE_ACK:
    case SCAN_TYPE_FIN:
    case SCAN_TYPE_XMAS:
        return "tcp";
    case SCAN_TYPE_UDP:
        return "udp";
    case SCAN_TYPE_ALL:
        return "tcp upd";
    default:
        return "Error";
    }
}

char *result2str(const t_result result) {
    switch (result) {
        case OPEN:
            return "open";
        case CLOSED:
            return "closed";
        case FILTERED:
            return "filtered";
        case OPEN_FILTERED:
            return "open|filtered";
        case UNFILTERED:
            return "unfiltered";
        default:
            return "error";
    }
}

bool is_ignored_state(t_result result, const scan_type_t scan_type) {
    switch (scan_type) {
        case SCAN_TYPE_SYN:
            return result == FILTERED;
        case SCAN_TYPE_ACK:
            return result == FILTERED;
        case SCAN_TYPE_NULL:
        case SCAN_TYPE_FIN:
        case SCAN_TYPE_XMAS:
            return result == OPEN_FILTERED;
        case SCAN_TYPE_UDP:
            return result == OPEN_FILTERED;
        case SCAN_TYPE_ALL:
            return result == OPEN_FILTERED || result == FILTERED;
        default:
            return false;
    }
}

uint16_t count_ports(const int *const ports) {
    uint16_t count = 0;

    for (int i = 0; i < PORTS_MAX && ports[i] != PORT_UNDEFINED; ++i)
        count++;
    return count;
}

uint8_t get_ports_status(const t_task *tasks, const uint16_t start_index, const uint16_t ports_num, const scan_type_t scan_type) {
    uint8_t status = 0;

    for (uint16_t i = start_index; i < start_index + ports_num; i++) {
        bool ignored_state = is_ignored_state(tasks[i].result, tasks[i].probe.scan_type);
        if (tasks[i].result == OPEN && (status & OPEN_PORTS) == 0) {
            status += OPEN_PORTS;
        } else if (tasks[i].result != OPEN && !ignored_state && (status & CLOSED_PORTS) == 0) {
            status += CLOSED_PORTS;
        } else if (ignored_state && (status & FILTERED_PORTS) == 0 /*&& DISPLAY_FILTERED*/) {
            status += FILTERED_PORTS;
        }
        if (scan_type == SCAN_TYPE_ALL)
            i += SCAN_NUMBER - 1;
    }
    return status;
}

void print_banner(char *str, bool newline) {
    if (newline)
        printf("\n");
    printf("%s port.s:\n", str);
    printf("Port       Service Name         Conclusion          Results\n");
    printf("----------------------------------------------------------------------\n");
}

void print_result(const t_task *tasks, const uint16_t i, const scan_type_t scan_type) {
    const uint16_t port = ntohs(tasks[i].probe.address.sin_port);
    char result_str[SCAN_NUMBER * 20] = {0};

    if (TEST_DISPLAY && scan_type != SCAN_TYPE_ALL)
        sprintf(result_str, "%s %s", scan_family2str(tasks[i].probe.scan_type), result2str(tasks[i].result));
    else
        sprintf(result_str, "%s(%s)", scan_type2str(tasks[i].probe.scan_type), result2str(tasks[i].result));

    if (scan_type == SCAN_TYPE_ALL) {
        for (uint16_t j = 1; j <= SCAN_NUMBER - 1; j++) {
            sprintf(result_str, "%s, %s(%s)", result_str, scan_type2str(tasks[i + j].probe.scan_type), result2str(tasks[i + j].result));
        }
    }
    
    const char *service_name_str = get_service_name(port, tasks[i].probe.scan_type == SCAN_TYPE_UDP ? "udp" : "tcp");
    const char *conclusion_str = result2str(tasks[i].result);
    if (TEST_DISPLAY && scan_type != SCAN_TYPE_ALL)
        printf("%d %s %s\n", port, result_str, service_name_str);
    else
        printf("%-10d %-20s %-19s %s\n", port, service_name_str, conclusion_str, result_str);
}

bool is_result_to_print(const t_task *tasks, const uint8_t port_status, const scan_type_t scan_type, const uint16_t i) {
    uint8_t current_ports = 0;
    if (port_status & OPEN_PORTS)
        current_ports = OPEN_PORTS;
    else if (port_status & CLOSED_PORTS)
        current_ports = CLOSED_PORTS;
    else if (port_status & FILTERED_PORTS)
        current_ports = FILTERED_PORTS;

    t_result result = tasks[i].result;
    if (result == OPEN && current_ports == OPEN_PORTS)
        return true;
    else if (!is_ignored_state(result, scan_type) && result != OPEN && current_ports == CLOSED_PORTS)
        return true;
    else if (is_ignored_state(result, scan_type) && current_ports == FILTERED_PORTS)
        return true;
    return false;

}

void print_results(t_task *tasks, const uint16_t num_tasks, const ft_nmap_config_t config) {
    const uint8_t scantype_offset = config.scan_type == SCAN_TYPE_ALL ? SCAN_NUMBER : 1; // 1 by default, 6 for SCAN_TYPE_ALL
    uint16_t ports_num = count_ports(config.ports) * scantype_offset;
    uint32_t current_task = 0;

    while (current_task + ports_num <= num_tasks) {
        printf("\n■■■■■■■■■■■■■■■■■■■■■■■\n█ IP: %-15s █\n■■■■■■■■■■■■■■■■■■■■■■■\n", inet_ntoa(tasks[current_task].probe.address.sin_addr));
        uint8_t ports_status = get_ports_status(tasks, current_task, ports_num, config.scan_type);
        uint16_t filtered_count = 0;

        if (ports_status & OPEN_PORTS)
            print_banner("Open", false);
        else if (ports_status & CLOSED_PORTS)
            print_banner("Closed/Unfiltered", false);
        else
            print_banner("Filtered", false);

        for (uint16_t i = current_task; i < current_task + ports_num; i += scantype_offset) {
            if (!DISPLAY_FILTERED && ports_status == FILTERED_PORTS && is_ignored_state(tasks[i].result, tasks[i].probe.scan_type))
                filtered_count++;
            else if (is_result_to_print(tasks, ports_status, config.scan_type, i))
                print_result(tasks, i, config.scan_type);
            if (i >= current_task + ports_num - scantype_offset) {
                if (ports_status & OPEN_PORTS)
                    ports_status -= OPEN_PORTS;
                else if (ports_status & CLOSED_PORTS)
                    ports_status -= CLOSED_PORTS;
                else if (ports_status & FILTERED_PORTS)
                    ports_status -= FILTERED_PORTS;
                if (ports_status) {
                    i = current_task - scantype_offset;
                    if (ports_status & CLOSED_PORTS) {
                        print_banner("Closed/Unfiltered", true);
                    } else if (ports_status & FILTERED_PORTS) {
                        print_banner("Filtered", true);
                    }
                }
            }
        }
        if (filtered_count > 0 && !DISPLAY_FILTERED)
            printf("<%d port.s ignored>\n", filtered_count);
        current_task += ports_num;
    }
}