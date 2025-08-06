#pragma once

#include "external.h"
#include "macro.h"
#include "type.h"

/* ---- prototypes ------------------------------------------------ */

// error
void safely_exit(const int code);
void print_error_and_exit(const char *const message);

// args
void print_help_and_exit(void);
bool is_number(const char *const s);
ft_nmap_args_t parse_arguments(const int argc, char *const argv[]);

// memory
void *safe_malloc_or_exit(const size_t size);
void memory_manager_cleanup(void);
