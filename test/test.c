#include "munit.h"
#include "../include/all.h"
#include <stdint.h>

/*
 * Test suite for safe memory allocation functions
 */

/* Test safe_malloc with valid sizes */
static MunitResult test_safe_malloc_valid(const MunitParameter params[], void* user_data)
{
    (void)params;
    (void)user_data;
    
    /* Test allocation of various sizes */
    void *ptr1 = safe_malloc(1);
    munit_assert_not_null(ptr1);
    safe_free((void**)&ptr1);
    munit_assert_null(ptr1);
    
    void *ptr2 = safe_malloc(1024);
    munit_assert_not_null(ptr2);
    safe_free((void**)&ptr2);
    munit_assert_null(ptr2);
    
    void *ptr3 = safe_malloc(sizeof(int) * 100);
    munit_assert_not_null(ptr3);
    safe_free((void**)&ptr3);
    munit_assert_null(ptr3);
    
    return MUNIT_OK;
}

/* Test safe_calloc with valid parameters */
static MunitResult test_safe_calloc_valid(const MunitParameter params[], void* user_data)
{
    (void)params;
    (void)user_data;
    
    /* Test calloc with different sizes */
    int *ptr1 = (int*)safe_calloc(10, sizeof(int));
    munit_assert_not_null(ptr1);
    
    /* Verify memory is zero-initialized */
    for (int i = 0; i < 10; i++)
    {
        munit_assert_int(ptr1[i], ==, 0);
    }
    safe_free((void**)&ptr1);
    munit_assert_null(ptr1);
    
    char *ptr2 = (char*)safe_calloc(100, sizeof(char));
    munit_assert_not_null(ptr2);
    
    /* Verify zero-initialization */
    for (int i = 0; i < 100; i++)
    {
        munit_assert_char(ptr2[i], ==, '\0');
    }
    safe_free((void**)&ptr2);
    munit_assert_null(ptr2);
    
    return MUNIT_OK;
}

/* Test safe_realloc functionality */
static MunitResult test_safe_realloc_valid(const MunitParameter params[], void* user_data)
{
    (void)params;
    (void)user_data;
    
    /* Start with malloc */
    int *ptr = (int*)safe_malloc(sizeof(int) * 5);
    munit_assert_not_null(ptr);
    
    /* Initialize data */
    for (int i = 0; i < 5; i++)
    {
        ptr[i] = i + 1;
    }
    
    /* Expand with realloc */
    ptr = (int*)safe_realloc(ptr, sizeof(int) * 10);
    munit_assert_not_null(ptr);
    
    /* Verify original data is preserved */
    for (int i = 0; i < 5; i++)
    {
        munit_assert_int(ptr[i], ==, i + 1);
    }
    
    /* Shrink with realloc */
    ptr = (int*)safe_realloc(ptr, sizeof(int) * 3);
    munit_assert_not_null(ptr);
    
    /* Verify data is still there */
    for (int i = 0; i < 3; i++)
    {
        munit_assert_int(ptr[i], ==, i + 1);
    }
    
    /* Test realloc with size 0 (should free and return NULL) */
    ptr = (int*)safe_realloc(ptr, 0);
    munit_assert_null(ptr);
    
    /* Test realloc with NULL pointer (should behave like malloc) */
    ptr = (int*)safe_realloc(NULL, sizeof(int) * 5);
    munit_assert_not_null(ptr);
    safe_free((void**)&ptr);
    
    return MUNIT_OK;
}

/* Test safe_free functionality */
static MunitResult test_safe_free_valid(const MunitParameter params[], void* user_data)
{
    (void)params;
    (void)user_data;
    
    /* Test normal free */
    void *ptr = safe_malloc(100);
    munit_assert_not_null(ptr);
    safe_free(&ptr);
    munit_assert_null(ptr);
    
    /* Test freeing NULL pointer */
    ptr = NULL;
    safe_free(&ptr); /* Should not crash */
    munit_assert_null(ptr);
    
    /* Test freeing with NULL double pointer */
    safe_free(NULL); /* Should not crash */
    
    /* Test double free protection */
    ptr = safe_malloc(50);
    safe_free(&ptr);
    munit_assert_null(ptr);
    safe_free(&ptr); /* Should not crash */
    munit_assert_null(ptr);
    
    return MUNIT_OK;
}

/* Test memory operations work together */
static MunitResult test_memory_operations_integration(const MunitParameter params[], void* user_data)
{
    (void)params;
    (void)user_data;
    
    /* Allocate with calloc */
    int *arr = (int*)safe_calloc(5, sizeof(int));
    munit_assert_not_null(arr);
    
    /* Verify zero-initialization */
    for (int i = 0; i < 5; i++)
    {
        munit_assert_int(arr[i], ==, 0);
    }
    
    /* Fill with data */
    for (int i = 0; i < 5; i++)
    {
        arr[i] = (i + 1) * 10;
    }
    
    /* Expand with realloc */
    arr = (int*)safe_realloc(arr, sizeof(int) * 10);
    munit_assert_not_null(arr);
    
    /* Verify data preservation */
    for (int i = 0; i < 5; i++)
    {
        munit_assert_int(arr[i], ==, (i + 1) * 10);
    }
    
    /* Clean up */
    safe_free((void**)&arr);
    munit_assert_null(arr);
    
    return MUNIT_OK;
}

/* Test suite definition */
static MunitTest safe_malloc_tests[] = {
    { "/safe_malloc_valid", test_safe_malloc_valid, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/safe_calloc_valid", test_safe_calloc_valid, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/safe_realloc_valid", test_safe_realloc_valid, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/safe_free_valid", test_safe_free_valid, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { "/memory_integration", test_memory_operations_integration, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

static const MunitSuite safe_malloc_suite = {
    "/safe_malloc", safe_malloc_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE
};

int main(int argc, char* argv[])
{
    return munit_suite_main(&safe_malloc_suite, NULL, argc, argv);
}