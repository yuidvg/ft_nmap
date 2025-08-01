#define _GNU_SOURCE
#include <arpa/inet.h>
#include <getopt.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "scanner.h"

/* -------------------------------------------------------------
 * Shared job queue (very small, simple implementation).
 * -----------------------------------------------------------*/
typedef struct
{
    const int   *ports;      /* pointer to immutable array */
    const size_t port_count; /* total number of ports */

    size_t          next_index; /* protected by mutex */
    pthread_mutex_t mutex;
} job_queue_t;

/* -------------------------------------------------------------
 * Worker context passed to each scanning thread.
 * -----------------------------------------------------------*/
typedef struct
{
    job_queue_t   *queue;
    port_state_t  *results;
    const char  *ip;
} worker_ctx_t;

/* -------------------------------------------------------------
 * Thread routine – fetch a port from the queue and run a SYN scan.
 * -----------------------------------------------------------*/
static void *scan_worker(void *data)
{
    worker_ctx_t          *const ctx = (worker_ctx_t *)data;
    const syn_scan_config_t cfg      = { .target_ip = ctx->ip, .timeout_ms = 1000 };

    while (true)
    {
        pthread_mutex_lock(&ctx->queue->mutex);
        if (ctx->queue->next_index >= ctx->queue->port_count)
        {
            pthread_mutex_unlock(&ctx->queue->mutex);
            break;
        }
        const size_t index = ctx->queue->next_index++;
        pthread_mutex_unlock(&ctx->queue->mutex);

        const int    port         = ctx->queue->ports[index];
        const port_state_t port_st  = syn_scan_port(cfg, port);
        ctx->results[index]       = port_st;
    }
    return NULL;
}

/* -------------------------------------------------------------
 * Entry point.
 * -----------------------------------------------------------*/
int main(int argc, char *argv[])
{
    /* One mutable local for eventual exit code (rule 9) */
    int exit_code = EXIT_FAILURE;

    /* Seed RNG once for src port randomisation */
    srand((unsigned int)time(NULL));

    /* -------------------- parse CLI -------------------- */
    const char *ip_arg      = NULL;
    const char *ports_arg   = NULL;
    int         speedup_val = 1;

    static struct option long_opts[] = {
        { "ip", required_argument, 0, 'i' },
        { "ports", required_argument, 0, 'p' },
        { "speedup", required_argument, 0, 's' },
        { "help", no_argument, 0, 'h' },
        { 0, 0, 0, 0 }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1)
    {
        if (opt == 'i')
        {
            ip_arg = optarg;
        }
        else if (opt == 'p')
        {
            ports_arg = optarg;
        }
        else if (opt == 's')
        {
            speedup_val = atoi(optarg);
        }
        else if (opt == 'h')
        {
            printf("ft_nmap minimal SYN scanner\n");
            printf("--ip <IPv4>            target IP address (required)\n");
            printf("--ports <range>        port range like 1-1024 (optional)\n");
            printf("--speedup <n>          number of threads (default 1, max 250)\n");
            exit(EXIT_SUCCESS);
        }
    }

    if (ip_arg == NULL)
    {
        fprintf(stderr, "Error: --ip is required.\n");
        return exit_code;
    }

    /* parse port range */
    int start_port = 1;
    int end_port   = 1024;
    if (ports_arg != NULL)
    {
        if (sscanf(ports_arg, "%d-%d", &start_port, &end_port) != 2)
        {
            fprintf(stderr, "Invalid --ports format. Use start-end (e.g., 20-80).\n");
            return exit_code;
        }
    }

    if (start_port < 1) start_port = 1;
    if (end_port > 65535) end_port = 65535;
    if (end_port < start_port)
    {
        fprintf(stderr, "Invalid port range.\n");
        return exit_code;
    }

    const size_t port_count = (size_t)(end_port - start_port + 1);
    int *const   ports      = calloc(port_count, sizeof(int));
    if (ports == NULL)
    {
        perror("calloc");
        return exit_code;
    }
    for (size_t i = 0; i < port_count; ++i)
    {
        ports[i] = start_port + (int)i;
    }

    if (speedup_val < 1) speedup_val = 1;
    if (speedup_val > 250) speedup_val = 250;

    /* -------------------- thread setup -------------------- */
    port_state_t *const results = calloc(port_count, sizeof(port_state_t));
    if (results == NULL)
    {
        perror("calloc");
        free(ports);
        return exit_code;
    }

    job_queue_t queue = {
        .ports      = ports,
        .port_count = port_count,
        .next_index = 0,
        .mutex      = PTHREAD_MUTEX_INITIALIZER
    };

    worker_ctx_t *const ctxs = calloc((size_t)speedup_val, sizeof(worker_ctx_t));
    pthread_t  *const tids = calloc((size_t)speedup_val, sizeof(pthread_t));

    for (int i = 0; i < speedup_val; ++i)
    {
        ctxs[i].queue   = &queue;
        ctxs[i].results = results;
        ctxs[i].ip      = ip_arg;
        pthread_create(&tids[i], NULL, scan_worker, &ctxs[i]);
    }

    for (int i = 0; i < speedup_val; ++i)
    {
        pthread_join(tids[i], NULL);
    }

    /* -------------------- reporting -------------------- */
    printf("Port  State\n");
    printf("--------------\n");
    for (size_t i = 0; i < port_count; ++i)
    {
        const char *state_str = "filtered";
        if (results[i] == PORT_STATE_OPEN) state_str = "open";
        else if (results[i] == PORT_STATE_CLOSED) state_str = "closed";
        printf("%5d %s\n", ports[i], state_str);
    }

    exit_code = EXIT_SUCCESS;

    /* cleanup */
    free(tids);
    free(ctxs);
    free(results);
    free(ports);
    return exit_code;
}
