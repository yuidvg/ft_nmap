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
