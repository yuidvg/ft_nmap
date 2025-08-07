#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#define STR_LEN   128

typedef struct Task {
    int id;
    void* (*function)(void*);
    char result[STR_LEN];

    struct Task* next;  // Linked list pointer
} Task;


typedef struct {
    Task* head;

    int num_tasks;
    int num_threads;
    int task_index;

    pthread_mutex_t queue_mutex;
} TaskManager;


void* thread_function_1(void* arg) { 
    Task* task = (Task*)arg;
    usleep(100000);
    snprintf(task->result, STR_LEN, "Thread %ld ran function 1 for task %d", pthread_self(), task->id);
    printf("%s\n", task->result);
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
        pthread_mutex_lock(&manager->queue_mutex);

        Task* task = manager->head;
        if (!task) {
            pthread_mutex_unlock(&manager->queue_mutex);
            break;
        }

        // Remove from list
        manager->head = task->next;

        pthread_mutex_unlock(&manager->queue_mutex);

        task->function(task);  // do the task
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

    Task* prev = NULL;

    for (int i = 0; i < manager.num_tasks; ++i) {
        Task* task = malloc(sizeof(Task));
        if (!task) {
            perror("malloc");
            return 1;
        }

        task->id = i;
        task->function = (i % 3 == 0) ? thread_function_1 :
                        (i % 3 == 1) ? thread_function_2 :
                                    thread_function_3;
        memset(task->result, 0, STR_LEN);
        task->next = NULL;

        if (prev) {
            prev->next = task;
        } else {
            manager.head = task;
        }

        prev = task;
    }


    // Start the workers
    pthread_mutex_init(&manager.queue_mutex, NULL);

    pthread_t* threads = malloc(manager.num_threads * sizeof(pthread_t));
    if (!threads) {
        perror("Memory allocation for threads failed");
        free(manager.head);//do cleanly
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
    Task* current = manager.head;
    while (current) {
        printf("Task %d: %s\n", current->id, current->result);
        current = current->next;
    }
    current = manager.head;
    while (current) {
        Task* next = current->next;
        free(current);
        current = next;
    }

    free(threads);

    pthread_mutex_destroy(&manager.queue_mutex);

    return 0;
}
