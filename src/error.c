#include "internal.h"

void safely_exit(const int code)
{
    exit(code);
}

void print_error_and_exit(const char *const message)
{
    fprintf(stderr, "Error: %s\n", message);
    safely_exit(EXIT_FAILURE);
}

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