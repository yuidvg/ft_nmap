#include "internal.h"

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

////////// CORE //////////
t_task *build_tasks(const probe_t *probes, const uint16_t num_tasks)
{
    // create the same number of tasks as probes
    t_task *tasks = malloc(sizeof(t_task) * num_tasks);
    if (!tasks)
    {
        perror("Failed to allocate memory for tasks");
        exit(EXIT_FAILURE);
    }
    for (uint16_t i = 0; i < num_tasks; i++)
    {
        tasks[i].probe = probes[i];
        tasks[i].status = CREATED;
        tasks[i].result = FILTERED;// change the default result in relation to the scan type instead of doing it in print_results
    }
    return tasks;
}

void *worker(void* arg) {
    t_thread_args *args = (t_thread_args*)arg;
    bool is_receiver = false;

    while (args->task_index < args->num_tasks || is_receiver) {
        if (pthread_mutex_trylock(&queue_mutex) == 0) {// Before sending a task, we check if there are any responses to handle
            bool check_message = true;
            while (check_message || is_receiver) {
                check_message = check_for_answer(args->tasks, args->handle, args->num_tasks);
                if (is_receiver && (time(NULL) - args->finish_time) > TIMEOUT)
                    return NULL;
            }
            pthread_mutex_unlock(&queue_mutex);
        }
        pthread_mutex_lock(&queue_mutex);
        const uint16_t my_index = args->task_index++;
        pthread_mutex_unlock(&queue_mutex);
        args->tasks[my_index].start = time(NULL);
        send_packet(args->local_ip, args->sock, args->tasks[my_index].probe);
        args->tasks[my_index].status = SENT;
        if (args->task_index >= args->num_tasks && !args->all_tasks_sent) {
            args->all_tasks_sent = true;
            args->finish_time = time(NULL);
            is_receiver = true; // This thread will be the receiver, checking for responses until the timeout
            printf("[*] All tasks sent, waiting for responses...\n");
        }
    }
    return NULL;
}

////////// TEMPORARY //////////
probe_t *build_probes(uint16_t num_tasks)
{
    // Create an array of probes with different scan types
    probe_t *probes = malloc(sizeof(probe_t) * num_tasks);
    if (!probes)
    {
        perror("Failed to allocate memory for probes");
        exit(EXIT_FAILURE);
    }
    // initialize the ports and ips
    const uint16_t ports[] = {80, 81, 444, 443};
    const char *ips[] = {"45.33.32.156", "45.33.32.156", "8.8.8.8", "8.8.8.8"};
    // Initialize the probes with different scan types
    for (uint16_t i = 0; i < 4; i++)
    {
        probes[i].scan_type = SCAN_TYPE_SYN;
        probes[i].address.sin_family = AF_INET;
        probes[i].address.sin_port = htons(ports[i]);
        probes[i].address.sin_addr.s_addr = inet_addr(ips[i]);
    }
    for (uint16_t i = 4; i < num_tasks; i++)
    {
        probes[i].scan_type = SCAN_TYPE_SYN;
        probes[i].address.sin_family = AF_INET;
        probes[i].address.sin_port = htons(80 + i);                    // Use the same ports for simplicity
        probes[i].address.sin_addr.s_addr = inet_addr("45.33.32.156"); // Use the same IPs for simplicity
    }
    return probes;
}
