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

// void          print_help_and_exit(void);
// bool          is_number(const char *const s);
// ft_nmap_args_t  parse_arguments(const int argc, char *const argv[]);

/* ------------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    /* Parse command-line options (pure) */
    const ft_nmap_args_t opts = parse_arguments(argc, argv);

    if (opts.help)
    {
        print_help_and_exit();
    }
    else
    {
        /* Print the parsed arguments for debugging/verification */
        printf("Parsed arguments:\n");
        printf("  help: %s\n", opts.help ? "true" : "false");
        printf("  file_path: %s\n", opts.file_path ? opts.file_path : "(null)");
        printf("  scan_type: %d\n", opts.scan_type);
        printf("  speedup: %d\n", opts.speedup);

        printf("  ip_addresses:\n");
        for (size_t i = 0; i < IP_ADDRESSES_MAX; ++i)
        {
            if (opts.ip_addresses[i] != NULL)
            {
                printf("    [%zu]: %s\n", i, opts.ip_addresses[i]);
            }
        }

        printf("  ports:\n");
        for (size_t i = 0; i < PORTS_MAX; ++i)
        {
            if (opts.ports[i] != PORT_UNDEFINED)
            {
                printf("    [%zu]: %d\n", i, opts.ports[i]);
            }
        }
    }

    return EXIT_SUCCESS;
}
