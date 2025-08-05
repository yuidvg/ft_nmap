#include "../src/internal.h"
#include <stdio.h>
#include <string.h>

// Example function that uses the memory manager
static void demonstrate_basic_usage(void)
{
    printf("=== Basic Memory Management Example ===\n");

    // Initialize memory manager (optional - it auto-initializes on first use)
    const malloc_result_t init_result = memory_manager_init();
    if (!init_result.succeeded)
    {
        printf("Failed to initialize memory manager\n");
        return;
    }

    printf("Memory manager initialized: %s\n",
           is_memory_manager_initialized() ? "true" : "false");

    // Allocate various sizes of memory
    const size_t sizes[] = {64, 128, 256, 512, 1024};
    const size_t num_allocations = sizeof(sizes) / sizeof(sizes[0]);

    for (size_t i = 0; i < num_allocations; ++i)
    {
        const malloc_result_t result = safe_malloc(sizes[i]);
        if (result.succeeded)
        {
            printf("Successfully allocated %zu bytes at %p\n",
                   sizes[i], result.ptr);

            // Use the memory - write some test data
            if (sizes[i] >= sizeof(int))
            {
                *((int *)result.ptr) = (int)i;
                printf("  Wrote test value: %d\n", *((int *)result.ptr));
            }
        }
        else
        {
            printf("Failed to allocate %zu bytes\n", sizes[i]);
        }
    }

    printf("All allocations completed. Memory will be automatically freed on cleanup.\n");
}

// Example of error handling
static void demonstrate_error_handling(void)
{
    printf("\n=== Error Handling Example ===\n");

    // Try to allocate zero bytes (should fail)
    const malloc_result_t zero_result = safe_malloc(0);
    if (zero_result.succeeded)
    {
        printf("Unexpected: zero-size allocation succeeded\n");
    }
    else
    {
        printf("Correctly rejected zero-size allocation\n");
    }

    // Try to allocate a very large amount (may fail on some systems)
    const size_t huge_size = SIZE_MAX / 2;
    const malloc_result_t huge_result = safe_malloc(huge_size);
    if (huge_result.succeeded)
    {
        printf("Large allocation (%zu bytes) succeeded at %p\n",
               huge_size, huge_result.ptr);
    }
    else
    {
        printf("Large allocation (%zu bytes) failed (expected on most systems)\n",
               huge_size);
    }
}

// Example of using allocated memory for strings
static void demonstrate_string_usage(void)
{
    printf("\n=== String Usage Example ===\n");

    const char *const test_strings[] = {
        "Hello, World!",
        "Thread-safe memory management",
        "Functional programming in C",
        "No more memory leaks!"
    };
    const size_t num_strings = sizeof(test_strings) / sizeof(test_strings[0]);

    for (size_t i = 0; i < num_strings; ++i)
    {
        const size_t string_length = strlen(test_strings[i]);
        const size_t buffer_size = string_length + 1; // +1 for null terminator

        const malloc_result_t result = safe_malloc(buffer_size);
        if (result.succeeded)
        {
            // Copy the string to our allocated memory
            strncpy((char *)result.ptr, test_strings[i], buffer_size);
            ((char *)result.ptr)[string_length] = '\0'; // Ensure null termination

            printf("Allocated and copied string %zu: \"%s\"\n",
                   i + 1, (char *)result.ptr);
        }
        else
        {
            printf("Failed to allocate memory for string %zu\n", i + 1);
        }
    }
}

int main(void)
{
    printf("Thread-Safe Memory Manager Demo\n");
    printf("===============================\n");

    // Demonstrate basic usage
    demonstrate_basic_usage();

    // Demonstrate error handling
    demonstrate_error_handling();

    // Demonstrate string usage
    demonstrate_string_usage();

    // Clean up all allocated memory at once
    printf("\n=== Cleanup ===\n");
    printf("Cleaning up all allocated memory...\n");
    memory_manager_cleanup();

    printf("Memory manager initialized after cleanup: %s\n",
           is_memory_manager_initialized() ? "true" : "false");

    printf("\nDemo completed successfully!\n");
    printf("All memory has been automatically freed - no memory leaks!\n");

    return 0;
}