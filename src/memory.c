#include "internal.h"

// Global memory manager instance (hidden from external access)
static memory_manager_t g_memory_manager = {
    .head = NULL,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .initialized = false
};

// Pure function to create a new memory block
static memory_block_t *create_memory_block(void *const ptr, const size_t size, memory_block_t *const next)
{
    memory_block_t *const block = malloc(sizeof(memory_block_t));
    if (block == NULL)
    {
        return NULL;
    }

    // Initialize the mutable fields before making them immutable conceptually
    *((void **)&block->ptr) = ptr;
    *((size_t *)&block->size) = size;
    *((memory_block_t **)&block->next) = next;

    return block;
}

// Pure predicate function to check initialization
bool is_memory_manager_initialized(void)
{
    return g_memory_manager.initialized;
}

// Initialize the memory manager (side effect wrapped in result type)
malloc_result_t memory_manager_init(void)
{
    if (g_memory_manager.initialized)
    {
        return (malloc_result_t){
            .succeeded = true,
            .ptr = NULL
        };
    }

    // Initialize mutex (this is a side effect, but necessary for thread safety)
    const int mutex_result = pthread_mutex_init(&g_memory_manager.mutex, NULL);
    if (mutex_result != 0)
    {
        return (malloc_result_t){
            .succeeded = false,
            .ptr = NULL
        };
    }

    // Mark as initialized (mutation of global state, but encapsulated)
    *((bool *)&g_memory_manager.initialized) = true;

    return (malloc_result_t){
        .succeeded = true,
        .ptr = NULL
    };
}

// Thread-safe malloc wrapper
malloc_result_t safe_malloc(const size_t size)
{
    // Validate input
    if (size == 0)
    {
        return (malloc_result_t){
            .succeeded = false,
            .ptr = NULL
        };
    }

    if (!g_memory_manager.initialized)
    {
        const malloc_result_t init_result = memory_manager_init();
        if (!init_result.succeeded)
        {
            return (malloc_result_t){
                .succeeded = false,
                .ptr = NULL
            };
        }
    }

    // Allocate memory
    void *const ptr = malloc(size);
    if (ptr == NULL)
    {
        return (malloc_result_t){
            .succeeded = false,
            .ptr = NULL
        };
    }

    // Thread-safe addition to the memory list
    const int lock_result = pthread_mutex_lock(&g_memory_manager.mutex);
    if (lock_result != 0)
    {
        free(ptr);
        return (malloc_result_t){
            .succeeded = false,
            .ptr = NULL
        };
    }

    // Create new memory block and add to list
    memory_block_t *const new_block = create_memory_block(ptr, size, g_memory_manager.head);
    if (new_block == NULL)
    {
        pthread_mutex_unlock(&g_memory_manager.mutex);
        free(ptr);
        return (malloc_result_t){
            .succeeded = false,
            .ptr = NULL
        };
    }

    // Update head (mutation of global state, but thread-safe)
    g_memory_manager.head = new_block;

    const int unlock_result = pthread_mutex_unlock(&g_memory_manager.mutex);
    if (unlock_result != 0)
    {
        // This is a critical error, but we've already allocated
        // The cleanup function will still work
        return (malloc_result_t){
            .succeeded = true,
            .ptr = ptr
        };
    }

    return (malloc_result_t){
        .succeeded = true,
        .ptr = ptr
    };
}

// Clean up all allocated memory (side effect, but necessary for resource management)
void memory_manager_cleanup(void)
{
    if (!g_memory_manager.initialized)
    {
        return;
    }

    const int lock_result = pthread_mutex_lock(&g_memory_manager.mutex);
    if (lock_result != 0)
    {
        // Even if we can't lock, try to free what we can
        // This is not ideal, but better than leaking memory
    }

    // Free all allocated blocks
    memory_block_t *current = g_memory_manager.head;
    while (current != NULL)
    {
        memory_block_t *const next = current->next;

        // Free the actual allocated memory
        if (current->ptr != NULL)
        {
            free(current->ptr);
        }

        // Free the block structure itself
        free(current);

        current = next;
    }

    // Reset the head
    g_memory_manager.head = NULL;

    if (lock_result == 0)
    {
        pthread_mutex_unlock(&g_memory_manager.mutex);
    }

    // Destroy the mutex
    pthread_mutex_destroy(&g_memory_manager.mutex);

    // Mark as uninitialized
    *((bool *)&g_memory_manager.initialized) = false;
}