#include "internal.h"

#define OPEN_PORTS 1
#define CLOSED_PORTS 1 << 1
#define FILTERED_PORTS 1 << 2
#define SCAN_NUMBER 6

char *service_name(const uint16_t port, const char *protocol) {
    struct servent *se;
    se = getservbyport(htons(port), protocol); // "tcp" or "udp"
    return se ? se->s_name : "Unassigned";
}

char *scan_type2str(const scan_type_t scan_type)
{
    switch (scan_type)
    {
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

char *result2str(const t_result result)
{
    switch (result) {
        case OPEN:
            return "Open";
        case CLOSED:
            return "Closed";
        case FILTERED:
            return "Filtered";
        case OPEN_FILTERED:
            return "Open|Filtered";
        case UNFILTERED:
            return "Unfiltered";
        default:
            return "Error";
    }
}

bool is_ignored_state(t_result result, const scan_type_t scan_type) {
    switch (scan_type) {
        case SCAN_TYPE_SYN:
            return result == CLOSED || result == FILTERED;
        case SCAN_TYPE_ACK:
            return result == FILTERED;
        case SCAN_TYPE_NULL:
        case SCAN_TYPE_FIN:
        case SCAN_TYPE_XMAS:
            return result == OPEN_FILTERED;
        case SCAN_TYPE_UDP:
            return result == FILTERED;
        default:
            return false;
    }
}

uint16_t count_ports(const int *const ports) {
    uint16_t count = 0;
    for (int i = 0; i < PORTS_MAX && ports[i] != PORT_UNDEFINED; ++i)
    {
        count++;
    }
    return count;
}

uint8_t get_ports_status(const t_task *tasks, const uint16_t start_index, const uint16_t ports_num, const scan_type_t scan_type) {
    uint8_t status = 0;
    for (uint16_t i = start_index; i < start_index + ports_num; i++) {
        bool ignored_state = is_ignored_state(tasks[i].result, tasks[i].probe.scan_type);
        if (scan_type == SCAN_TYPE_ALL) {
            for (uint16_t j = 1; j < SCAN_NUMBER; j++) {
                if (ignored_state)
                    ignored_state = is_ignored_state(tasks[i + j].result, tasks[i + j].probe.scan_type);
            }
        }
        if (tasks[i].result == OPEN && (status & OPEN_PORTS) == 0) {
            status += OPEN_PORTS;
        } else if (!ignored_state && (status & CLOSED_PORTS) == 0) {
            status += CLOSED_PORTS;
        } else if (ignored_state && (status & FILTERED_PORTS) == 0) {
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
    printf("%s ports:\n", str);
    printf("Port       Service Name         Conclusion          Results\n");
    printf("----------------------------------------------------------------------\n");
}

void print_result(const t_task *tasks, const uint16_t i, const scan_type_t scan_type) {
    const uint16_t port = ntohs(tasks[i].probe.address.sin_port);
    char result_str[20*SCAN_NUMBER];
    sprintf(result_str, "%s(%s)", scan_type2str(tasks[i].probe.scan_type), result2str(tasks[i].result));
    if (scan_type == SCAN_TYPE_ALL) {
        for (uint16_t j = 1; j <= SCAN_NUMBER - 1; j++) {
            sprintf(result_str, "%s, %s(%s)", result_str, scan_type2str(tasks[i + j].probe.scan_type), result2str(tasks[i + j].result));
        }
    }
    const char *service_name_str = service_name(port, scan_type == SCAN_TYPE_UDP ? "udp" : "tcp");//should we use both for SCAN_TYPE_ALL? - ALSO, THIS TAKES A LOT OF TIME, DO WITH THREADS AND IN A BETTER PLACE
    const char *conclusion_str = result2str(tasks[i].result);
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
    if (scan_type == SCAN_TYPE_ALL && result != OPEN) {
        result = FILTERED;
        for (uint16_t j = 1; j < SCAN_NUMBER; j++) {
            if (!is_ignored_state(tasks[i + j].result, tasks[i + j].probe.scan_type)) {
                result = CLOSED;
                break;
            }
        }
    }
    if (result == OPEN && current_ports == OPEN_PORTS) {
        return true;
    } else if (result == CLOSED && current_ports == CLOSED_PORTS) {
        return true;
    } else if (result == FILTERED && current_ports == FILTERED_PORTS) {
        //return false;
        return true;
    }
    return false;

}

void print_results(t_task *tasks, const uint16_t num_tasks, const ft_nmap_config_t config) {
    const uint8_t scantype_offset = config.scan_type == SCAN_TYPE_ALL ? SCAN_NUMBER : 1; // 1 by default, 6 for SCAN_TYPE_ALL
    uint16_t ports_num = count_ports(config.ports) * scantype_offset;
    uint32_t current_task = 0;

    while (current_task + ports_num <= num_tasks) {
        printf("\n---| IP: %s |---\n", inet_ntoa(tasks[current_task].probe.address.sin_addr));
        uint8_t ports_status = get_ports_status(tasks, current_task, ports_num, config.scan_type);
        //bool closed_pass = ports_status & CLOSED_PORTS;

        if (ports_status & OPEN_PORTS)
            print_banner("Open", false);//Change depending on the scan_type
        else if (ports_status & CLOSED_PORTS)
            print_banner("Closed/Unfiltered", false);
        else
            print_banner("Filtered", false);
        for (uint16_t i = current_task; i < current_task + ports_num; i++)
        {
            /*if ((tasks[i].result == OPEN && ports_status & OPEN_PORTS) ||
                (!is_ignored_state(tasks[i].result, tasks[i].probe.scan_type) && ports_status & CLOSED_PORTS) ||
                (is_ignored_state(tasks[i].result, tasks[i].probe.scan_type) && ports_status & FILTERED_PORTS))
            {
                print_result(tasks, i, config.scan_type);
            }*/
            if (is_result_to_print(tasks, ports_status, config.scan_type, i)) {
                print_result(tasks, i, config.scan_type);
            }
            if (i >= current_task + ports_num - scantype_offset) {
                if (ports_status & OPEN_PORTS)
                    ports_status -= OPEN_PORTS;
                else if (ports_status & CLOSED_PORTS)
                    ports_status -= CLOSED_PORTS;
                else if (ports_status & FILTERED_PORTS)
                    ports_status -= FILTERED_PORTS;
                if (ports_status) {
                    i = current_task;
                    if (ports_status & CLOSED_PORTS) {
                        print_banner("Closed/Unfiltered", true);
                    } else if (ports_status & FILTERED_PORTS) {
                        print_banner("Filtered", true);
                    }
                }
            }
            if (config.scan_type == SCAN_TYPE_ALL)
                i += SCAN_NUMBER - 1;
        }
        current_task += ports_num;
    }
}
//We should be able to go above PORTS_MAX in args

/*
FIRST AND FOREMOST DO THE CONCLUSION_STR
ADJUST THE LOGIC TO THE 3 STATES OPEN CLOSED FILTERED
*/