#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#define STR_LEN   128

typedef struct {
    int id;
    void* (*function)(void*);
} Task;

Task* tasks = NULL;
char** results = NULL;

int num_tasks = 0;
int num_threads = 0;
int task_index = 0;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

void* thread_function_1(void* arg) {
    int id = *(int*)arg;
    usleep(100000);
    snprintf(results[id], STR_LEN, "Thread %ld ran function 1 for task %d", pthread_self(), id);
    usleep(100000);
    return NULL;
}

void* thread_function_2(void* arg) {
    int id = *(int*)arg;
    usleep(300000);
    snprintf(results[id], STR_LEN, "Thread %ld ran function 2 for task %d", pthread_self(), id);
    usleep(300000);
    return NULL;
}

void* thread_function_3(void* arg) {
    int id = *(int*)arg;
    snprintf(results[id], STR_LEN, "Thread %ld ran function 3 for task %d", pthread_self(), id);
    return NULL;
}

void* worker(void* arg) {
    while (1) {
        Task task;

        pthread_mutex_lock(&queue_mutex);
        if (task_index >= num_tasks) {
            pthread_mutex_unlock(&queue_mutex);
            break;
        }
        task = tasks[task_index++];
        pthread_mutex_unlock(&queue_mutex);

        int task_id = task.id;
        task.function(&task_id);
    }
    return NULL;
}

//Needs better error management
int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <number_of_tasks> <number_of_threads>\n", argv[0]);
        return 1;
    }

    num_tasks = atoi(argv[1]);
    if (num_tasks <= 0) {
        fprintf(stderr, "Number of tasks must be a number greater than 0\n");
        return 1;
    }
    num_threads = atoi(argv[2]);
    if (num_threads < 0) {
        fprintf(stderr, "Number of threads must be a number greater than 0\n");
        return 1;
    }
    // --- Allocate tasks and results dynamically ---
    tasks = malloc(num_tasks * sizeof(Task));
    results = malloc(num_tasks * sizeof(char*));
    if (!tasks || !results) {
        perror("Memory allocation failed");
        return 1;
    }

    for (int i = 0; i < num_tasks; ++i) {
        results[i] = malloc(STR_LEN);
        if (!results[i]) {
            perror("Memory allocation for result string failed");
            return 1;
        }

        tasks[i].id = i;
        tasks[i].function = (i % 3 == 0) ? thread_function_1 : (i % 3 == 1) ? thread_function_2 : thread_function_3;
    }

    // --- Start worker threads ---
    pthread_t *threads = malloc((num_threads) * (sizeof(pthread_t)));

    for (int i = 0; i < num_threads; ++i) {
        int rc = pthread_create(&threads[i], NULL, worker, NULL);
        if (rc != 0) {
            fprintf(stderr, "Error creating thread %d: %s\n", i, strerror(rc));
            // handle cleanup or exit here
        }
    }

    // --- Main thread also acts as a worker ---
    worker(NULL);

    for (int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

    // --- Output results ---
    printf("\n--- Results ---\n");
    for (int i = 0; i < num_tasks; ++i) {
        printf("%s\n", results[i]);
        free(results[i]); // free each string
    }

    // --- Cleanup ---
    free(results);
    free(tasks);
    free(threads);
    pthread_mutex_destroy(&queue_mutex);

    return 0;
}
