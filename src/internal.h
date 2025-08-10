#pragma once

#include "external.h"
#include "macro.h"
#include "type.h"




// error
void safely_exit(const int code);
void print_error_and_exit(const char *const message);
void print_help_and_exit(void);

// args
ft_nmap_args_t parse_arguments(const int argc, char *const argv[]);
ft_nmap_config_t args2config(const ft_nmap_args_t args);
probe_t *config2probes(const ft_nmap_config_t config);
size_t count_probes(const probe_t *const probes);

// memory
void *safe_malloc_or_exit(const size_t size);
void memory_manager_cleanup(void);

// print
const char *scan_type2str(const scan_type_t scan_type);
void print_results(const t_task *tasks, const uint16_t num_tasks);

//receive
void handle_packet(t_task *tasks, const u_char *data, const uint16_t num_tasks);
bool check_for_answer(t_task *tasks, pcap_t *handle, const uint16_t num_tasks);

//send
unsigned short checksum(unsigned short *ptr, int nbytes);
void send_syn_packet(const char *local_ip, const int sock, const struct sockaddr_in test, const uint16_t i);
udp_packet_t get_udp_packet_checksumed(const udp_packet_t unchecksumed_packet);

// setup
int get_local_ip_and_iface(char *iface, char *ip);
int setup_socket();
pcap_t *setup_handle(const char *iface);
void setup_filter(pcap_t *handle);

// worker
t_task *build_tasks(const probe_t*probes, const uint16_t num_probes);
void *worker(void *arg);
probe_t*build_probes(const uint16_t num_probes);

// wrapper
char **ft_split_or_exit(char const *s, char c);