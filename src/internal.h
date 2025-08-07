#pragma once

#include "external.h"
#include "macro.h"
#include "type.h"

/* ---- prototypes ------------------------------------------------ */

// error
void safely_exit(const int code);
void print_error_and_exit(const char *const message);
void print_help_and_exit(void);

// args
ft_nmap_args_t parse_arguments(const int argc, char *const argv[]);
ft_nmap_config_t args2config(const ft_nmap_args_t args);
probe_info_t *config2probe_infos(const ft_nmap_config_t config);
size_t count_probe_infos(const probe_info_t *const probe_infos);

// memory
void *safe_malloc_or_exit(const size_t size);
void memory_manager_cleanup(void);

// print
const char *scan_type2str(const scan_type_t scan_type);

// wrapper
char **ft_split_or_exit(char const *s, char c);