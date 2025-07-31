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

typedef struct
{
    const char  *const ports;       /* NULL → default 1-1024                 */
    const char  *const ip_address;  /* NULL unless provided                  */
    const char  *const file_path;   /* NULL unless provided                  */
    const char  *const scan_types;  /* NULL → default (all)                  */
    const int          speedup;     /* 0 → default (no threading)            */
    const bool         show_help;   /* true  → show help & exit              */
} Scan_options;

/* ---- Forward declarations ------------------------------------------------ */
static void          print_help_and_exit(void);
static bool          is_number(const char *const s);
static Scan_options  parse_arguments(const int argc, char *const argv[]);

/* ------------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    /* Parse command-line options (pure) */
    const Scan_options opts = parse_arguments(argc, argv);

    /* Handle --help early */
    if (opts.show_help)
    {
        print_help_and_exit();
    }

    /* For now we simply echo the parsed configuration – the actual
       scanning logic will come later. */
    printf("Scan configuration\n");
    printf("  ports      : %s\n", opts.ports);
    printf("  ip         : %s\n", opts.ip_address ? opts.ip_address : "<none>");
    printf("  file       : %s\n", opts.file_path  ? opts.file_path  : "<none>");
    printf("  scan types : %s\n", opts.scan_types);
    printf("  speedup    : %d\n", opts.speedup);

    return 0;
}

/* ------------------------------------------------------------------------- */
static void print_help_and_exit(void)
{
    puts("ft_nmap [OPTIONS]");
    puts("  --help                 Print this help screen");
    puts("  --ports   <RANGE/LIST> Ports to scan (e.g. 1-10 or 1,2,3 or 1,5-15)");
    puts("  --ip      <ADDRESS>    IPv4 address or hostname to scan");
    puts("  --file    <FILE>       File containing IPv4 addresses/hostnames to scan");
    puts("  --speedup <0-250>      Number of parallel threads to use");
    puts("  --scan    <TYPES>      Scan types (SYN, NULL, FIN, XMAS, ACK, UDP)");
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

/* Core parser – returns an immutable `Scan_options` value. On any error,
   prints a message to stderr and exits with failure status. */
static Scan_options parse_arguments(const int argc, char *const argv[])
{
    /* Defaults */
    const char *ports_default      = "1-1024";
    const char *scan_all_default   = "SYN,NULL,FIN,XMAS,ACK,UDP";

    /* Working variables (mutable internally, result immutable) */
    const char *ports      = ports_default;
    const char *ip_address = NULL;
    const char *file_path  = NULL;
    const char *scan_types = scan_all_default;
    int         speedup    = 0;
    bool        show_help  = false;

    /* Walk argv */
    int i = 1; /* skip program name */
    while (i < argc)
    {
        const char *const arg = argv[i];

        if (strcmp(arg, "--help") == 0)
        {
            show_help = true;
            ++i;
        }
        else if (strcmp(arg, "--ports") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "Error: --ports expects a value\n");
                exit(EXIT_FAILURE);
            }
            ports = argv[i + 1];
            i += 2;
        }
        else if (strcmp(arg, "--ip") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "Error: --ip expects a value\n");
                exit(EXIT_FAILURE);
            }
            ip_address = argv[i + 1];
            i += 2;
        }
        else if (strcmp(arg, "--file") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "Error: --file expects a value\n");
                exit(EXIT_FAILURE);
            }
            file_path = argv[i + 1];
            i += 2;
        }
        else if (strcmp(arg, "--speedup") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "Error: --speedup expects a value\n");
                exit(EXIT_FAILURE);
            }
            const char *const value = argv[i + 1];
            if (!is_number(value))
            {
                fprintf(stderr, "Error: --speedup value must be numeric\n");
                exit(EXIT_FAILURE);
            }
            speedup = atoi(value);
            if (speedup < 0 || speedup > 250)
            {
                fprintf(stderr, "Error: --speedup must be between 0 and 250\n");
                exit(EXIT_FAILURE);
            }
            i += 2;
        }
        else if (strcmp(arg, "--scan") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "Error: --scan expects a value\n");
                exit(EXIT_FAILURE);
            }
            scan_types = argv[i + 1];
            i += 2;
        }
        else
        {
            fprintf(stderr, "Error: Unknown argument '%s'\n", arg);
            exit(EXIT_FAILURE);
        }
    }

    /* Basic validation */
    if ((ip_address == NULL && file_path == NULL) && !show_help)
    {
        fprintf(stderr, "Error: either --ip or --file must be provided\n");
        exit(EXIT_FAILURE);
    }
    if (ip_address != NULL && file_path != NULL)
    {
        fprintf(stderr, "Error: --ip and --file are mutually exclusive\n");
        exit(EXIT_FAILURE);
    }

    /* Return immutable result */
    const Scan_options result = {
        .ports      = ports,
        .ip_address = ip_address,
        .file_path  = file_path,
        .scan_types = scan_types,
        .speedup    = speedup,
        .show_help  = show_help
    };

    return result;
}
