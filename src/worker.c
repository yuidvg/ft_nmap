#include "internal.h"

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

float get_time_diff(const struct timespec start, const struct timespec end) {
    return (float)(end.tv_sec - start.tv_sec) + (float)(end.tv_nsec - start.tv_nsec) / 1e9;
}

t_task *build_tasks(const probe_t *probes, const uint16_t num_tasks) {
    t_task *tasks = safe_malloc_or_exit(sizeof(t_task) * num_tasks);
    if (!tasks)
    {
        perror("Failed to allocate memory for tasks");
        exit(EXIT_FAILURE);
    }
    for (uint16_t i = 0; i < num_tasks; i++)
    {
        tasks[i].probe = probes[i];
        tasks[i].status = CREATED;
        if (probes[i].scan_type == SCAN_TYPE_SYN || probes[i].scan_type == SCAN_TYPE_ACK)
            tasks[i].result = FILTERED;
        else
            tasks[i].result = OPEN_FILTERED;
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
                clock_gettime(CLOCK_REALTIME, &args->finish_time);
                is_receiver = true;
            }
            pthread_mutex_unlock(&queue_mutex);
            break;
        }
        const uint16_t my_index = args->task_index++;
        pthread_mutex_unlock(&queue_mutex);

        args->tasks[my_index].start = time(NULL);
        send_packet(args->local_ip, args->sock, args->tasks[my_index].probe);
        args->tasks[my_index].status = SENT;
    }

    //Dedicated thread (the one that sent the last task) handle the responses until the timeout
    if (is_receiver) {
        printf("All tasks sent - Scanning took %fs - Waiting for responses...\n", get_time_diff(args->start_time, args->finish_time));
        bool check_message = true;
        struct timespec current_time;
        clock_gettime(CLOCK_REALTIME, &current_time);
        while (check_message || get_time_diff(args->finish_time, current_time) < TIMEOUT) {
            check_message = check_for_answer(args->tasks, args->handle, args->num_tasks);
            clock_gettime(CLOCK_REALTIME, &current_time);
            usleep(10000);
        }
        printf("Timeout over - printing results:\n");
    }
    return NULL;
}