#ifndef ALL_H
#define ALL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Safe memory allocation functions */
void *safe_malloc(size_t size);
void *safe_calloc(size_t nmemb, size_t size);
void *safe_realloc(void *ptr, size_t size);
void  safe_free(void **ptr);

#endif /* ALL_H */
