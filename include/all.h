#ifndef ALL_H
#define ALL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

/* Safe memory allocation functions */
void *safe_malloc(size_t size);
void *safe_calloc(size_t nmemb, size_t size);
void *safe_realloc(void *ptr, size_t size);
void  safe_free(void **ptr);

/* Thread-safe malloc wrapper with tracking */
void *tracked_malloc(size_t size);
void *tracked_calloc(size_t nmemb, size_t size);
void *tracked_realloc(void *ptr, size_t size);
void  tracked_free_all(void);
void  tracked_cleanup(void);

#endif /* ALL_H */
