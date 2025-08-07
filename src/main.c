#include "internal.h"
#include "type.h"

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

////////// TEMPORARY //////////
t_probe *build_probes(uint16_t num_tasks) {
    // Create an array of probes with different scan types
    t_probe *probes = malloc(sizeof(t_probe) * num_tasks);
    if (!probes) {
        perror("Failed to allocate memory for probes");
        exit(EXIT_FAILURE);
    }
    //initialize the ports and ips
    const uint16_t ports[] = {80, 81, 444, 443};
    const char *ips[] = {"45.33.32.156", "45.33.32.156", "8.8.8.8", "8.8.8.8"};
    // Initialize the probes with different scan types
    for (uint16_t i = 0; i < 4; i++) {
        probes[i].scan_type = SYN;
        probes[i].addr.sin_family = AF_INET;
        probes[i].addr.sin_port = htons(ports[i]);
        probes[i].addr.sin_addr.s_addr = inet_addr(ips[i]);
    }
    for (uint16_t i = 4; i < num_tasks; i++) {
        probes[i].scan_type = SYN;
        probes[i].addr.sin_family = AF_INET;
        probes[i].addr.sin_port = htons(80 + i); // Use the same ports for simplicity
        probes[i].addr.sin_addr.s_addr = inet_addr("45.33.32.156"); // Use the same IPs for simplicity
    }
    return probes;
}

////////// CORE //////////
t_task *build_tasks(const t_probe *probes, const uint16_t num_tasks) {
    //create the same number of tasks as probes
    t_task *tasks = malloc(sizeof(t_task) * num_tasks);
    if (!tasks) {
        perror("Failed to allocate memory for tasks");
        exit(EXIT_FAILURE);
    }
    for (uint16_t i = 0; i < num_tasks; i++) {
        tasks[i].probe = probes[i];
        tasks[i].status = CREATED;
        tasks[i].result = FILTERED;
        tasks[i].function = send_syn_packet;
    }
    return tasks;
}

void* worker(void* arg) {
    t_thread_args *args = (t_thread_args*)arg;
    bool is_receiver = false;

    while (1) {
        if (pthread_mutex_trylock(&queue_mutex) == 0) {
            bool check_message = true;
            while (check_message) {// Before sending a task, we check if there are any responses to handle
                check_message = check_for_answer(args->tasks, args->handle, args->num_tasks);
            }
            pthread_mutex_unlock(&queue_mutex);
        }
        pthread_mutex_lock(&queue_mutex);
        if (args->task_index >= args->num_tasks) {// When all tasks are sent, we choose a thread to be the receiver and we lock the timer
            if (!args->all_tasks_sent) {
                args->all_tasks_sent = true;
                args->finish_time = time(NULL);
                is_receiver = true;
            }
            pthread_mutex_unlock(&queue_mutex);
            break;
        }
        const uint16_t my_index = args->task_index++;
        pthread_mutex_unlock(&queue_mutex);

        args->tasks[my_index].start = time(NULL);
        args->tasks[my_index].function(args->local_ip, args->sock, args->tasks[my_index].probe.addr, my_index);
        args->tasks[my_index].status = SENT;
    }

    //Dedicated thread (the one that sent the last task) handle the responses until the timeout
    if (is_receiver) {
        printf("[*] All tasks sent. Waiting for responses...\n");
        bool check_message = true;
        while (check_message || (time(NULL) - args->finish_time) < TIMEOUT) {
            check_message = check_for_answer(args->tasks, args->handle, args->num_tasks);
        }
        //for some reason if we add a sleep here we miss some responses
        printf("[*] Receiver thread finished.\n");
    }
    return NULL;
}

int main(int argc, char *argv[]) {
  const ft_nmap_args_t args = parse_arguments(argc, argv);

    /* Print the parsed arguments for debugging/verification */
    printf("Parsed arguments:\n");
    printf("  file_path: %s\n", args.file_path ? args.file_path : "(null)");
    printf("  scan_type: %d\n", args.scan_type);
    printf("  speedup: %d\n", args.speedup);

    printf("  ip_address: %s\n", args.ip_address ? args.ip_address : "(null)");
    printf("  file_path: %s\n", args.file_path ? args.file_path : "(null)");

    printf("  ports:\n");
    for (size_t i = 0; i < PORTS_MAX; ++i)
    {
        if (args.ports[i] != PORT_UNDEFINED)
        {
            printf("    [%zu]: %d\n", i, args.ports[i]);
        }
    }
    ft_nmap_config_t config = args2config(args);
    const probe_info_t *const probe_infos = config2probe_infos(config);

    /* Print probe_infos for debugging/verification */
    printf("\nProbe infos:\n");

    for (size_t i = 0; probe_infos[i].scan_type != 0; ++i)
    {
        const probe_info_t probe = probe_infos[i];
        const char *const ip_str = inet_ntoa(probe.sockaddr.sin_addr);
        const int port = ntohs(probe.sockaddr.sin_port);

        printf("  [%zu]: %s:%d (scan_type: %s)\n", i, ip_str, port, scan_type2str(probe.scan_type));
    }
  
// TEMPORARY PARSING
    if (argc != 2) {
        fprintf(stderr, "Need one arg\n");
        return 1;
    }

    const int num_threads = atoi(argv[1]);
    if (num_threads < 0) {
        fprintf(stderr, "Number of threads must be a >= to 0\n");
        return 1;
    }

// SETUP
    t_thread_args args;
    args.num_tasks = 50;    // TEMPORARY
    const t_probe *probes = build_probes(args.num_tasks);
    if (DEBUG) printf("[*] Created %d probes\n", args.num_tasks);

    args.tasks = build_tasks(probes, args.num_tasks);
    if (DEBUG) printf("[*] Created %d tasks\n", args.num_tasks);
    if (get_local_ip_and_iface(args.iface, args.local_ip) != 0) {
        perror("Could not determine local IP/interface");
        exit(EXIT_FAILURE);
    }
    printf("[*] Using interface: %s (%s)\n", args.iface, args.local_ip);

    args.sock = setup_socket();
    args.handle = setup_handle(args.iface);
    setup_filter(args.handle); // Needs to be improved A LOT

// SENDING (AND RECEIVING) THE PACKETS

    pthread_t *threads = malloc((num_threads) * (sizeof(pthread_t)));
    if (!threads) {
        printf("Failed to allocate memory for threads\n");
        exit(EXIT_FAILURE);
    }
    args.task_index = 0;
    args.all_tasks_sent = false;
    args.finish_time = 0;
    if (DEBUG) printf("[*] Starting %d threads...\n", num_threads);
    for (int i = 0; i < num_threads; ++i) {
        const int rc = pthread_create(&threads[i], NULL, worker, (void *)&args);
        if (rc != 0) {
            fprintf(stderr, "Error creating thread %d: %s\n", i, strerror(rc));
            //<handle error>!
        }
    }
    worker((void *)&args);
    for (int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

// RESULTS AND CLEANUP
    print_results(args.tasks, args.num_tasks); // Print the results of the scan

    pcap_close(args.handle);
    close(args.sock);
    pthread_mutex_destroy(&queue_mutex);
    return 0;
}

//sudo nmap -sS -p 80 scanme.nmap.org
//sudo ./build/bin/ft_nmap 5
