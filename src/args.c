#include "internal.h"
#include "macro.h"
#include "type.h"
#include <ctype.h>
#include <getopt.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void print_help_and_exit(void)
{
    printf("Help Screen\n"
           "ft_nmap\t[OPTIONS]\n");
    printf("\t%-10s %s\n", "--help", "Print this help screen");
    printf("\t%-10s %s\n", "--ports", "ports to scan (eg: 1-10 or 1,2,3 or 1,5-15)");
    printf("\t%-10s %s\n", "--ip", "ip addresses to scan in dot format");
    printf("\t%-10s %s\n", "--file", "File name containing IP addresses to scan,");
    printf("\t%-10s %s\n", "--speedup", "[250 max] number of parallel threads to use");
    printf("\t%-10s %s\n", "--scan", "SYN/NULL/FIN/XMAS/ACK/UDP");
    exit(EXIT_SUCCESS);
}


/* Helper function to parse port ranges and individual ports */
static void parse_port_string(const char *const port_str, int ports_out[PORTS_MAX])
{
    char *const ports_copy = strdup(port_str);
    if (ports_copy != NULL)
    {

        size_t port_count = 0;
        char *token = strtok_r(port_str, ",");

        while (token != NULL && port_count < PORTS_MAX)
        {
            /* Check if token contains a range (e.g., "1-10") */
            char *const dash_pos = strchr(token, '-');
            if (dash_pos != NULL)
            {
                /* Parse range */
                *dash_pos = '\0';
                const char *const start_str = token;
                const char *const end_str = dash_pos + 1;

                if (is_number(start_str) && is_number(end_str))
                {
                    const int start_port = atoi(start_str);
                    const int end_port = atoi(end_str);

                    for (int port = start_port; port <= end_port && port_count < PORTS_MAX; ++port)
                    {
                        if (port > 0 && port <= 65535)
                        {
                            ports_out[port_count++] = port;
                        }
                    }
                }
            }
            else
            {
                /* Parse single port */
                if (is_number(token))
                {
                    const int port = atoi(token);
                    if (port > 0 && port <= 65535)
                    {
                        ports_out[port_count++] = port;
                    }
                }
            }

            token = strtok(NULL, ",");
        }

        free(ports_copy);
    }
    else
    {
        return; /* Memory allocation failed */
    }

}

// /* Simple numeric check (ASCII decimal digits only) */
bool is_number(const char *const s)
{
    const char *p = s;
    while (*p != '\0')
    {
        if (!isdigit((unsigned char)*p))
        {
            return false;
        }
        ++p;
    }
    return (p != s);
}

/* Helper function to parse scan type from string */
static scan_type_t parse_scan_type_string(const char *const scan_str)
{
    if (scan_str != NULL)
    {
        if (strcmp(scan_str, "SYN") == 0)
        {
            return SCAN_TYPE_SYN;
        }
        else if (strcmp(scan_str, "NULL") == 0)
        {
            return SCAN_TYPE_NULL;
        }
        else if (strcmp(scan_str, "FIN") == 0)
        {
            return SCAN_TYPE_FIN;
        }
        else if (strcmp(scan_str, "XMAS") == 0)
        {
            return SCAN_TYPE_XMAS;
        }
        else if (strcmp(scan_str, "ACK") == 0)
        {
            return SCAN_TYPE_ACK;
        }
        else if (strcmp(scan_str, "UDP") == 0)
        {
            return SCAN_TYPE_UDP;
        }
        else
        {
            return SCAN_TYPE_ALL;
        }
    }
    else
        return SCAN_TYPE_ALL;
}

/* Core parser using getopt_long – returns an immutable ft_nmap_args_t value.
   On any error, prints a message to stderr and exits with failure status. */
ft_nmap_args_t parse_arguments(const int argc, char *const argv[])
{
    /* Temporary variables to collect parsed values */
    bool help = false;
    char *file_path = NULL;
    scan_type_t scan_type = SCAN_TYPE_ALL;
    int speedup = 1;
    int ports[PORTS_MAX];
    const char *ip_addresses[IP_ADDRESSES_MAX];

    /* Initialize arrays */
    for (size_t i = 0; i < PORTS_MAX; ++i)
    {
        ports[i] = PORT_UNDEFINED;
    }
    for (size_t i = 0; i < IP_ADDRESSES_MAX; ++i)
    {
        ip_addresses[i] = NULL;
    }

    /* Define long options */
    static const struct option long_options[] = {{"help", no_argument, NULL, 'h'},
                                                 {"ports", required_argument, NULL, 'p'},
                                                 {"ip", required_argument, NULL, 'i'},
                                                 {"file", required_argument, NULL, 'f'},
                                                 {"speedup", required_argument, NULL, 's'},
                                                 {"scan", required_argument, NULL, 'S'},
                                                 {0, 0, 0, 0}};

    /* Parse options */
    int option_index = 0;
    int c;
    size_t ip_count = 0;

    while ((c = getopt_long(argc, argv, "hp:i:f:s:S:", long_options, &option_index)) != -1)
    {
        switch (c)
        {
        case 'h':
            help = true;
            break;

        case 'p':
            parse_port_string(optarg, ports);
            break;

        case 'i':
            if (ip_count < IP_ADDRESSES_MAX)
            {
                ip_addresses[ip_count++] = optarg;
            }
            break;

        case 'f':
            if (file_path != NULL)
            {
                print_error_and_exit("Multiple --file arguments provided");
            }
            file_path = optarg;
            break;

        case 's':
            if (is_number(optarg))
            {
                const int parsed_speedup = atoi(optarg);
                if (parsed_speedup > 0 && parsed_speedup <= 250)
                {
                    speedup = parsed_speedup;
                }
                else
                {
                    print_error_and_exit("Speedup must be between 1 and 250");
                }
            }
            else
            {
                print_error_and_exit("Invalid speedup value");
            }
            break;

        case 'S':
            scan_type = parse_scan_type_string(optarg);
            break;

        case '?':
            /* getopt_long already printed an error message */
            exit(EXIT_FAILURE);
            break;

        default:
            print_error_and_exit("Unknown option");
            break;
        }
    }

    /* Create and return the immutable structure */
    ft_nmap_args_t args = {
        .help = help,
        .file_path = file_path,
        .scan_type = scan_type,
        .speedup = speedup,
    };

    /* Copy arrays */
    for (size_t i = 0; i < PORTS_MAX; ++i)
    {
        args.ports[i] = ports[i];
    }
    for (size_t i = 0; i < IP_ADDRESSES_MAX; ++i)
    {
        args.ip_addresses[i] = ip_addresses[i];
    }

    return args;
}
