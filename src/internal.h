#pragma once

#include "external.h"
#include "type.h"
#include "macro.h"

/* ---- prototypes ------------------------------------------------ */

// Argument parsing
void          print_help_and_exit(void);
bool          is_number(const char *const s);
ft_nmap_args_t  parse_arguments(const int argc, char *const argv[]);