#include <stdbool.h>

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
#include "type.h"

// void          print_help_and_exit(void);
// bool          is_number(const char *const s);
// ft_nmap_args_t  parse_arguments(const int argc, char *const argv[]);

/* ------------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    const ft_nmap_args_t args = parse_arguments(argc, argv);

    /* Print the parsed arguments for debugging/verification */
    printf("Parsed arguments:\n");
    printf("  file_path: %s\n", args.file_path ? args.file_path : "(null)");
    printf("  scan_type: %d\n", args.scan_type);
    printf("  speedup: %d\n", args.speedup);

    printf("  ip_address: %s\n", args.ip_address ? args.ip_address : "(null)");
    printf("  file_path: %s\n", args.file_path ? args.file_path : "(null)");

    printf("  ports:\n");
    for (size_t i = 0; i < PORTS_MAX; ++i)
    {
        if (args.ports[i] != PORT_UNDEFINED)
        {
            printf("    [%zu]: %d\n", i, args.ports[i]);
        }
    }
    ft_nmap_config_t config = args2config(args);
    const probe_info_t *const probe_infos = config2probe_infos(config);

    /* Print probe_infos for debugging/verification */
    printf("\nProbe infos:\n");

    for (size_t i = 0; probe_infos[i].scan_type != 0; ++i)
    {
        const probe_info_t probe = probe_infos[i];
        const char *const ip_str = inet_ntoa(probe.sockaddr.sin_addr);
        const int port = ntohs(probe.sockaddr.sin_port);

        printf("  [%zu]: %s:%d (scan_type: %s)\n", i, ip_str, port, scan_type2str(probe.scan_type));
    }
}
