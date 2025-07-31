#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

/*
 * Simple command-line parser for ft_nmap.
 * Supported options (see project spec):
 *   --help                         : print usage
 *   --ports   <STRING>             : ports to scan (e.g. 1-10 or 1,2,3 or 1,5-15)
 *   --ip      <IPV4 / HOSTNAME>    : single IPv4 address or hostname to scan
 *   --file    <PATH>               : path to file containing multiple targets
 *   --speedup <NUMBER   0-250>     : number of parallel threads
 *   --scan    <STRING>             : scan types, one of SYN/NULL/FIN/XMAS/ACK/UDP
 *
 * Design notes (functional-style flavour):
 *  ‑ Results are gathered into an immutable struct that is
 *    returned by value from the parser.
 *  ‑ All helper functions are pure – they depend only on their
 *    parameters and have no observable side-effects.
 *  ‑ Only the top-level `main` performs I/O / `exit`.
 */
/* ---- Forward declarations ------------------------------------------------ */
#include "internal.h"

void          print_help_and_exit(void);
bool          is_number(const char *const s);
ft_nmap_args_t  parse_arguments(const int argc, char *const argv[]);

/* ------------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    /* Parse command-line options (pure) */
    const ft_nmap_args_t opts = parse_arguments(argc, argv);

    /* Handle --help early */
    if (opts.help)
    {
        print_help_and_exit();
    }

    /* For now we simply echo the parsed configuration – the actual
       scanning logic will come later. */
    printf("Scan configuration\n");
    printf("  ports      : %s\n", opts.ports);
    printf("  ip         : %s\n", opts.ip_address ? opts.ip_address : "<none>");
    printf("  file       : %s\n", opts.file_path  ? opts.file_path  : "<none>");
    printf("  scan types : %s\n", opts.scan_type);
    printf("  speedup    : %d\n", opts.speedup);

    return 0;
}

/* ------------------------------------------------------------------------- */
static void print_help_and_exit(void)
{
    puts("ft_nmap [OPTIONS]");
    puts("  --help                 Print this help screen");
    puts("  --ports   <NUMBER/RANGED> Ports to scan (e.g. 1-10 or 1,2,3 or 1,5-15). The number of ports to scan cannot exceed 1024. default: 1-1024");
    puts("  --ip      <IP_ADDRESS>    IPv4 address or hostname to scan");
    puts("  --file    <FILE>       File containing IPv4 addresses/hostnames to scan");
    puts("  --speedup <NUMBER>      Number of parallel threads to use default: 0, max: 250");
    puts("  --scan    <TYPE>      Scan type (SYN/NULL/FIN/XMAS/ACK/UDP) default: SYN&NULL&FIN&XMAS&ACK&UDP");
    exit(EXIT_SUCCESS);
}

/* Simple numeric check (ASCII decimal digits only) */
static bool is_number(const char *const s)
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
    return (p != s); /* at least one digit */
}

static bool parse_help(const int argc, char *const argv[])
{
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "--help") == 0)
        {
            return true;
        }
    }
    return false;
}

static port_range_t parse_ports(const int argc, char *const argv[])
{
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "--ports") == 0)
        {
            if (is_number(argv[i + 1]))
            {
                return (port_range_t){.start = atoi(argv[i + 1]), .end = atoi(argv[i + 1])};
            }
            
            {
                
            }
        }
    }
    return (port_range_t){.start = 1, .end = 1024};
}

static char *parse_ipaddress(const int argc, char *const argv[])
{
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "--ip") == 0)
        {
            return argv[i + 1];
        }
    }
}

static char *parse_filepath(const int argc, char *const argv[])
{
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "--file") == 0)
        {
            return argv[i + 1];
        }
    }
}

static scan_type_t parse_scan_type(const int argc, char *const argv[])
{
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "--scan") == 0)
        {
            if (strcmp(argv[i + 1], "SYN") == 0)
            {
                return SCAN_TYPE_SYN;
            }
            else if (strcmp(argv[i + 1], "NULL") == 0)
            {
                return SCAN_TYPE_NULL;
            }
            else if (strcmp(argv[i + 1], "FIN") == 0)
            {
                return SCAN_TYPE_FIN;
            }
            else if (strcmp(argv[i + 1], "XMAS") == 0)
            {
                return SCAN_TYPE_XMAS;
            }
            else if (strcmp(argv[i + 1], "ACK") == 0)
            {
                return SCAN_TYPE_ACK;
            }
            else if (strcmp(argv[i + 1], "UDP") == 0)
            {
                return SCAN_TYPE_UDP;
            }
            else
            {
                return SCAN_TYPE_SYN | SCAN_TYPE_NULL | SCAN_TYPE_FIN | SCAN_TYPE_XMAS | SCAN_TYPE_ACK | SCAN_TYPE_UDP;
            }
        }
    }
}

static int parse_speedup(const int argc, char *const argv[])
{
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "--speedup") == 0)
        {
            return atoi(argv[i + 1]);
        }
    }
}



/* Core parser – returns an immutable `Scan_options` value. On any error,
   prints a message to stderr and exits with failure status. */
static ft_nmap_args_t parse_arguments(const int argc, char *const argv[])
{
    return (ft_nmap_args_t){
        .help = parse_help(argc, argv),
        .ports = parse_ports(argc, argv),
        .ip_address = parse_ipaddress(argc, argv),
        .file_path = parse_filepath(argc, argv),
        .scan_type = parse_scan_type(argc, argv),
        .speedup = parse_speedup(argc, argv),
    };
}
