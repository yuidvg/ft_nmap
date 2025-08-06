#include "all.h"
#include <stdint.h>

/*
 * Safe memory allocation suite for ft_nmap
 * 
 * These functions provide safer alternatives to standard memory allocation
 * functions with proper error handling and additional safety features.
 */

/**
 * Safe malloc wrapper that exits on allocation failure
 * @param size: Number of bytes to allocate
 * @return: Pointer to allocated memory (never NULL)
 */
void *safe_malloc(size_t size)
{
    if (size == 0)
    {
        fprintf(stderr, "Error: safe_malloc called with size 0\n");
        exit(EXIT_FAILURE);
    }

    void *ptr = malloc(size);
    if (ptr == NULL)
    {
        fprintf(stderr, "Error: Failed to allocate %zu bytes\n", size);
        exit(EXIT_FAILURE);
    }

    return ptr;
}

/**
 * Safe calloc wrapper that exits on allocation failure
 * @param nmemb: Number of elements to allocate
 * @param size: Size of each element in bytes
 * @return: Pointer to zero-initialized memory (never NULL)
 */
void *safe_calloc(size_t nmemb, size_t size)
{
    if (nmemb == 0 || size == 0)
    {
        fprintf(stderr, "Error: safe_calloc called with zero size\n");
        exit(EXIT_FAILURE);
    }

    /* Check for overflow in multiplication */
    if (nmemb > SIZE_MAX / size)
    {
        fprintf(stderr, "Error: safe_calloc size overflow\n");
        exit(EXIT_FAILURE);
    }

    void *ptr = calloc(nmemb, size);
    if (ptr == NULL)
    {
        fprintf(stderr, "Error: Failed to allocate %zu elements of %zu bytes\n", nmemb, size);
        exit(EXIT_FAILURE);
    }

    return ptr;
}

/**
 * Safe realloc wrapper that exits on allocation failure
 * @param ptr: Pointer to previously allocated memory (can be NULL)
 * @param size: New size in bytes
 * @return: Pointer to reallocated memory (never NULL unless size is 0)
 */
void *safe_realloc(void *ptr, size_t size)
{
    if (size == 0)
    {
        /* Standard behavior: free the pointer and return NULL */
        if (ptr != NULL)
        {
            free(ptr);
        }
        return NULL;
    }

    void *new_ptr = realloc(ptr, size);
    if (new_ptr == NULL)
    {
        fprintf(stderr, "Error: Failed to reallocate %zu bytes\n", size);
        exit(EXIT_FAILURE);
    }

    return new_ptr;
}

/**
 * Safe free that nullifies the pointer after freeing
 * @param ptr: Pointer to pointer to be freed and nullified
 */
void safe_free(void **ptr)
{
    if (ptr == NULL)
    {
        return; /* Nothing to do */
    }

    if (*ptr != NULL)
    {
        free(*ptr);
        *ptr = NULL;
    }
}

/*
 * Thread-safe malloc wrapper with allocation tracking
 * 
 * This section provides thread-safe memory allocation functions that track
 * all allocations and allow freeing everything at once.
 */

/* Structure to track allocated memory blocks */
typedef struct allocation_node {
    void *ptr;
    size_t size;
    struct allocation_node *next;
} allocation_node_t;

/* Global variables for tracking allocations */
static allocation_node_t *allocation_list = NULL;
static pthread_mutex_t allocation_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool tracker_initialized = false;

/**
 * Initialize the allocation tracker (called automatically)
 */
static void init_tracker(void)
{
    if (!tracker_initialized)
    {
        pthread_mutex_init(&allocation_mutex, NULL);
        allocation_list = NULL;
        tracker_initialized = true;
    }
}

/**
 * Add an allocation to the tracking list
 * @param ptr: Pointer to allocated memory
 * @param size: Size of allocation
 */
static void track_allocation(void *ptr, size_t size)
{
    if (ptr == NULL) return;

    init_tracker();

    allocation_node_t *node = malloc(sizeof(allocation_node_t));
    if (node == NULL)
    {
        fprintf(stderr, "Error: Failed to allocate tracking node\n");
        exit(EXIT_FAILURE);
    }

    node->ptr = ptr;
    node->size = size;

    pthread_mutex_lock(&allocation_mutex);
    node->next = allocation_list;
    allocation_list = node;
    pthread_mutex_unlock(&allocation_mutex);
}

/**
 * Remove an allocation from the tracking list
 * @param ptr: Pointer to memory being freed
 */
static void untrack_allocation(void *ptr)
{
    if (ptr == NULL) return;

    pthread_mutex_lock(&allocation_mutex);
    
    allocation_node_t *current = allocation_list;
    allocation_node_t *prev = NULL;

    while (current != NULL)
    {
        if (current->ptr == ptr)
        {
            if (prev == NULL)
            {
                allocation_list = current->next;
            }
            else
            {
                prev->next = current->next;
            }
            free(current);
            break;
        }
        prev = current;
        current = current->next;
    }

    pthread_mutex_unlock(&allocation_mutex);
}

/**
 * Thread-safe malloc that tracks allocations
 * @param size: Number of bytes to allocate
 * @return: Pointer to allocated memory (never NULL)
 */
void *tracked_malloc(size_t size)
{
    if (size == 0)
    {
        fprintf(stderr, "Error: tracked_malloc called with size 0\n");
        exit(EXIT_FAILURE);
    }

    void *ptr = malloc(size);
    if (ptr == NULL)
    {
        fprintf(stderr, "Error: Failed to allocate %zu bytes\n", size);
        exit(EXIT_FAILURE);
    }

    track_allocation(ptr, size);
    return ptr;
}

/**
 * Thread-safe calloc that tracks allocations
 * @param nmemb: Number of elements to allocate
 * @param size: Size of each element in bytes
 * @return: Pointer to zero-initialized memory (never NULL)
 */
void *tracked_calloc(size_t nmemb, size_t size)
{
    if (nmemb == 0 || size == 0)
    {
        fprintf(stderr, "Error: tracked_calloc called with zero size\n");
        exit(EXIT_FAILURE);
    }

    /* Check for overflow in multiplication */
    if (nmemb > SIZE_MAX / size)
    {
        fprintf(stderr, "Error: tracked_calloc size overflow\n");
        exit(EXIT_FAILURE);
    }

    void *ptr = calloc(nmemb, size);
    if (ptr == NULL)
    {
        fprintf(stderr, "Error: Failed to allocate %zu elements of %zu bytes\n", nmemb, size);
        exit(EXIT_FAILURE);
    }

    track_allocation(ptr, nmemb * size);
    return ptr;
}

/**
 * Thread-safe realloc that tracks allocations
 * @param ptr: Pointer to previously allocated memory (can be NULL)
 * @param size: New size in bytes
 * @return: Pointer to reallocated memory (never NULL unless size is 0)
 */
void *tracked_realloc(void *ptr, size_t size)
{
    if (size == 0)
    {
        /* Standard behavior: free the pointer and return NULL */
        if (ptr != NULL)
        {
            untrack_allocation(ptr);
            free(ptr);
        }
        return NULL;
    }

    /* Remove old tracking before realloc */
    if (ptr != NULL)
    {
        untrack_allocation(ptr);
    }

    void *new_ptr = realloc(ptr, size);
    if (new_ptr == NULL)
    {
        fprintf(stderr, "Error: Failed to reallocate %zu bytes\n", size);
        exit(EXIT_FAILURE);
    }

    /* Track the new allocation */
    track_allocation(new_ptr, size);

    return new_ptr;
}

/**
 * Thread-safe free that removes allocation from tracking
 * @param ptr: Pointer to memory to be freed
 */
void tracked_free(void *ptr)
{
    if (ptr == NULL) return;

    untrack_allocation(ptr);
    free(ptr);
}

/**
 * Free all tracked allocations at once
 */
void tracked_free_all(void)
{
    pthread_mutex_lock(&allocation_mutex);

    allocation_node_t *current = allocation_list;
    while (current != NULL)
    {
        allocation_node_t *next = current->next;
        free(current->ptr);
        free(current);
        current = next;
    }
    allocation_list = NULL;

    pthread_mutex_unlock(&allocation_mutex);
}

/**
 * Cleanup the allocation tracker and free all resources
 */
void tracked_cleanup(void)
{
    tracked_free_all();
    pthread_mutex_destroy(&allocation_mutex);
    tracker_initialized = false;
}