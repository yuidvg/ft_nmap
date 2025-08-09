#include "internal.h"
#include "type.h"

extern pthread_mutex_t queue_mutex;

int main(int argc, char *argv[])
{
    const ft_nmap_args_t command_line_args = parse_arguments(argc, argv);

    /* Print the parsed arguments for debugging/verification */
    printf("Parsed arguments:\n");
    printf("  file_path: %s\n", command_line_args.file_path ? command_line_args.file_path : "(null)");
    printf("  scan_type: %d\n", command_line_args.scan_type);
    printf("  speedup: %d\n", command_line_args.speedup);

    printf("  ip_address: %s\n", command_line_args.ip_address ? command_line_args.ip_address : "(null)");
    printf("  file_path: %s\n", command_line_args.file_path ? command_line_args.file_path : "(null)");

    // printf("  ports:\n");
    // for (size_t i = 0; i < PORTS_MAX && command_line_args.ports[i] != PORT_UNDEFINED; ++i)
    //     printf("    [%zu]: %d\n", i, command_line_args.ports[i]);

    ft_nmap_config_t config = args2config(command_line_args);
    const probe_t *const probes = config2probes(config);

    /* Print probe_infos for debugging/verification */
    printf("\nProbe infos:\n");

    for (size_t i = 0; probes[i].scan_type != 0; ++i)
    {
        const probe_t probe = probes[i];
        const char *const ip_str = inet_ntoa(probe.address.sin_addr);

        const int port = ntohs(probe.address.sin_port);

        printf("  [%zu]: %s:%d (scan_type: %s)\n", i, ip_str, port, scan_type2str(probe.scan_type));
    }

    // SETUP
    t_thread_args args;
    args.num_tasks = count_probes(probes);
    if (DEBUG)
        printf("[*] Created %d probes\n", args.num_tasks);

    args.tasks = build_tasks(probes, args.num_tasks);
    if (DEBUG)
        printf("[*] Created %d tasks\n", args.num_tasks);
    if (get_local_ip_and_iface(args.iface, args.local_ip) != 0)
    {
        perror("Could not determine local IP/interface");
        exit(EXIT_FAILURE);
    }
    printf("[*] Using interface: %s (%s)\n", args.iface, args.local_ip);

    args.sock = setup_socket();
    args.handle = setup_handle(args.iface);
    setup_filter(args.handle); // Needs to be improved A LOT

    // SENDING (AND RECEIVING) THE PACKETS

    pthread_t *threads = malloc((config.number_of_threads) * (sizeof(pthread_t)));
    if (!threads)
    {
        printf("Failed to allocate memory for threads\n");
        exit(EXIT_FAILURE);
    }
    args.task_index = 0;
    args.all_tasks_sent = false;
    args.finish_time = 0;
    if (DEBUG)
        printf("[*] Starting %d threads...\n", config.number_of_threads);
    for (int i = 0; i < config.number_of_threads; ++i)
    {
        const int rc = pthread_create(&threads[i], NULL, worker, (void *)&args);
        if (rc != 0)
        {
            fprintf(stderr, "Error creating thread %d: %s\n", i, strerror(rc));
            //<handle error>!
        }
    }
    worker((void *)&args);
    for (int i = 0; i < config.number_of_threads; ++i)
    {
        pthread_join(threads[i], NULL);
    }

    // RESULTS AND CLEANUP
    print_results(args.tasks, args.num_tasks); // Print the results of the scan

    pcap_close(args.handle);
    close(args.sock);
    pthread_mutex_destroy(&queue_mutex);
    return 0;
}

// sudo nmap -sS -p 80 scanme.nmap.org
// sudo ./build/bin/ft_nmap 5
