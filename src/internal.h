#pragma once

#include "external.h"
#include "type.h"
#include "macro.h"

/* ---- prototypes ------------------------------------------------ */

// Argument parsing
void          print_help_and_exit(void);
bool          is_number(const char *const s);
ft_nmap_args_t  parse_arguments(const int argc, char *const argv[]);

// Memory management
malloc_result_t memory_manager_init(void);
malloc_result_t safe_malloc(const size_t size);
void            memory_manager_cleanup(void);
bool            is_memory_manager_initialized(void);