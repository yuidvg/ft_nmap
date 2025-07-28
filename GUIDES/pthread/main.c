#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#define STR_LEN   128

typedef struct {
    int id;
    void* (*function)(void*);
    char result[STR_LEN];  // result buffer inside the task struct
} Task;

typedef struct {
    Task* tasks;

    int num_tasks;
    int num_threads;
    int task_index;

    pthread_mutex_t queue_mutex;
} TaskManager;


void* thread_function_1(void* arg) {
    Task* task = (Task*)arg;
    usleep(100000);
    snprintf(task->result, STR_LEN, "Thread %ld ran function 1 for task %d", pthread_self(), task->id);
    usleep(100000);
    return NULL;
}

void* thread_function_2(void* arg) {
    Task* task = (Task*)arg;
    usleep(300000);
    snprintf(task->result, STR_LEN, "Thread %ld ran function 2 for task %d", pthread_self(), task->id);
    usleep(300000);
    return NULL;
}

void* thread_function_3(void* arg) {
    Task* task = (Task*)arg;
    snprintf(task->result, STR_LEN, "Thread %ld ran function 3 for task %d", pthread_self(), task->id);
    return NULL;
}

void* worker(void* arg) {
    TaskManager* manager = (TaskManager*)arg;

    while (1) {
        Task* task_ptr = NULL;

        pthread_mutex_lock(&manager->queue_mutex);
        if (manager->task_index >= manager->num_tasks) {
            pthread_mutex_unlock(&manager->queue_mutex);
            break;
        }
        task_ptr = &manager->tasks[manager->task_index++];
        pthread_mutex_unlock(&manager->queue_mutex);

        task_ptr->function(task_ptr);
    }

    return NULL;
}

//Needs better error management
int main(int argc, char* argv[]) {
    // Parsing
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <number_of_tasks> <number_of_threads>\n", argv[0]);
        return 1;
    }

    TaskManager manager = {0};//initialize the struct with NULL values

    manager.num_tasks = atoi(argv[1]);
    if (manager.num_tasks <= 0) {
        fprintf(stderr, "Number of tasks must be a number and > 0\n");
        return 1;
    }
    manager.num_threads = atoi(argv[2]);
    if (manager.num_threads < 0) {
        fprintf(stderr, "Number of threads must be a number and >= 0\n");
        return 1;
    }

    manager.tasks = malloc(manager.num_tasks * sizeof(Task));
    if (!manager.tasks) {
        perror("Memory allocation failed");
        return 1;
    }

    for (int i = 0; i < manager.num_tasks; ++i) {
        manager.tasks[i].id = i;
        manager.tasks[i].function = (i % 3 == 0) ? thread_function_1 :
                                   (i % 3 == 1) ? thread_function_2 :
                                                  thread_function_3;
        memset(manager.tasks[i].result, 0, STR_LEN);
    }

    // Start the workers
    pthread_mutex_init(&manager.queue_mutex, NULL);

    pthread_t* threads = malloc(manager.num_threads * sizeof(pthread_t));
    if (!threads) {
        perror("Memory allocation for threads failed");
        free(manager.tasks);
        return 1;
    }

    for (int i = 0; i < manager.num_threads; ++i) {
        int rc = pthread_create(&threads[i], NULL, worker, &manager);
        if (rc != 0) {
            fprintf(stderr, "Error creating thread %d: %s\n", i, strerror(rc));
            // Add cleanup here
        }
    }

    worker(&manager);// Main thread also does work

    // Wait for the workers to be done
    for (int i = 0; i < manager.num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

    // Print the results
    printf("\n--- Results ---\n");
    for (int i = 0; i < manager.num_tasks; ++i) {
        printf("%s\n", manager.tasks[i].result);
    }

    free(manager.tasks);
    free(threads);

    pthread_mutex_destroy(&manager.queue_mutex);

    return 0;
}
