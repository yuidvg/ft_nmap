#include "internal.h"
#include <string.h>

// Global memory manager instance (hidden from external access)
static memory_manager_t g_memory_manager = {.head = nullptr, .mutex = PTHREAD_MUTEX_INITIALIZER};

// Automatic initialization using constructor attribute
[[gnu::constructor]] static void memory_manager_auto_init(void)
{
    pthread_mutex_init(&g_memory_manager.mutex, nullptr);
}

// Automatic cleanup using destructor attribute
[[gnu::destructor(101)]] static void memory_manager_auto_cleanup(void)
{
    pthread_mutex_destroy(&g_memory_manager.mutex);

    // Free all allocated blocks
    memory_block_t *current = g_memory_manager.head;
    while (current != nullptr)
    {
        memory_block_t *const next = current->next;

        // Free the actual allocated memory
        if (current->ptr != nullptr)
        {
            free(current->ptr);
        }

        // Free the block structure itself
        free(current);

        current = next;
    }
    // Reset the head
    g_memory_manager.head = NULL;
}

// Pure function to create a new memory block
static memory_block_t *create_memory_block_or_exit(void *const ptr, const size_t size, memory_block_t *const next)
{
    memory_block_t *const block = malloc(sizeof(memory_block_t));
    if (block != nullptr)
    {
        // Initialize the mutable fields before making them immutable conceptually
        *((void **)&block->ptr) = ptr;
        *((size_t *)&block->size) = size;
        *((memory_block_t **)&block->next) = next;

        return block;
    }
    else
    {
        free(ptr);
        print_error_and_exit("Failed to allocate memory block");
    }
    return nullptr;
}

// Thread-safe malloc wrapper
void *safe_malloc_or_exit(const size_t size)
{
    if (size > 0)
    {
        void *const ptr = malloc(size);
        if (ptr != nullptr)
        {
            const int lock_result = pthread_mutex_lock(&g_memory_manager.mutex);
            if (lock_result == 0)
            {
                // Create new memory block and add to list
                memory_block_t *const new_block = create_memory_block_or_exit(ptr, size, g_memory_manager.head);
                if (new_block != nullptr)
                {
                    g_memory_manager.head = new_block;
                    // now free on ptr is unneeded
                    const int unlock_result = pthread_mutex_unlock(&g_memory_manager.mutex);
                    if (unlock_result == 0)
                    {
                        return ptr;
                    }
                    else
                    {
                        print_error_and_exit("kernel mutex unlock error");
                    }
                }
                else
                {
                    pthread_mutex_unlock(&g_memory_manager.mutex);
                    free(ptr);
                    print_error_and_exit("Failed to create memory block");
                }
            }
            else
            {
                free(ptr);
                print_error_and_exit("kernel mutex error");
            }
        }
        else
        {
            print_error_and_exit("Failed to allocate memory");
        }
    }
    return nullptr;
}

// Manual cleanup function for testing purposes
void memory_manager_cleanup(void)
{
    const int lock_result = pthread_mutex_lock(&g_memory_manager.mutex);
    if (lock_result == 0)
    {
        // Free all allocated blocks
        memory_block_t *current = g_memory_manager.head;
        while (current != nullptr)
        {
            memory_block_t *const next = current->next;

            // Free the actual allocated memory
            if (current->ptr != nullptr)
            {
                free(current->ptr);
            }

            // Free the block structure itself
            free(current);

            current = next;
        }

        // Reset the head
        g_memory_manager.head = nullptr;

        const int unlock_result = pthread_mutex_unlock(&g_memory_manager.mutex);
        if (unlock_result != 0)
        {
            print_error_and_exit("kernel mutex unlock error during cleanup");
        }
    }
    else
    {
        print_error_and_exit("kernel mutex error during cleanup");
    }
}
