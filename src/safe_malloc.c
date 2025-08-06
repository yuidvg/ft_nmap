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