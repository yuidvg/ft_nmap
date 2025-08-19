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
        if (probes[i].scan_type == SCAN_TYPE_SYN || probes[i].scan_type == SCAN_TYPE_ACK || probes[i].scan_type == SCAN_TYPE_UDP)
            tasks[i].result = FILTERED;
        else
            tasks[i].result = OPEN_FILTERED;
    }
    return tasks;
}

//TRIED TO REWORK WORKER() THIS BUT IT CREATED ISSUES
/*
void *worker(void* arg) {
    t_thread_args *args = (t_thread_args*)arg;
    bool is_receiver = false;

    while (!args->all_tasks_sent || is_receiver) {
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
*/

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
        send_packet(args->local_ip, args->sock, args->tasks[my_index].probe);
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