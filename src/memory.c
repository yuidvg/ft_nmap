#include "internal.h"

// Memory management types
typedef struct memory_block
{
    void *ptr;
    struct memory_block *next;
} memory_block_t;

typedef struct
{
    memory_block_t *head;
    pthread_mutex_t mutex;
} memory_manager_t;

static memory_manager_t g_memory_manager = {.head = NULL, .mutex = PTHREAD_MUTEX_INITIALIZER};

__attribute__((constructor)) static void init_memory_manager_or_exit(void)
{
    const int result = pthread_mutex_init(&g_memory_manager.mutex, NULL);
    if (result != 0)
    {
        print_error_and_exit("Failed to initialize mutex");
    }
}

static void atomiccally_or_exit(bool (*func)(void *), void *arg)
{
    const int lock_result = pthread_mutex_lock(&g_memory_manager.mutex);
    if (lock_result == 0)
    {
        const bool error = func(arg);
        const int unlock_result = pthread_mutex_unlock(&g_memory_manager.mutex);
        if (error || unlock_result != 0)
        {
            print_error_and_exit("kernel error in atomic operation");
        }
    }
    else
    {
        print_error_and_exit("kernel mutex error");
    }
}

__attribute__((destructor)) static void cleanup_memory_manager(void)
{
    pthread_mutex_destroy(&g_memory_manager.mutex);

    // Free all allocated blocks
    memory_block_t *current = g_memory_manager.head;
    while (current != NULL)
    {
        memory_block_t *const next = current->next;

        if (current->ptr != NULL)
        {
            free(current->ptr);
        }

        free(current);

        current = next;
    }
    g_memory_manager.head = NULL;
}

static memory_block_t *create_memory_block_or_exit(void *const ptr)
{
    memory_block_t *const block = malloc(sizeof(memory_block_t));
    if (block != NULL)
    {
        block->ptr = ptr;
        block->next = g_memory_manager.head;
        return block;
    }
    else
    {
        free(ptr);
        print_error_and_exit("Failed to allocate memory block");
    }
    return NULL;
}

static bool register_memory_block_or_exit_unatomiccaly(void *const ptr)
{
    memory_block_t *const new_block = create_memory_block_or_exit(ptr);
    if (new_block != NULL)
    {
        g_memory_manager.head = new_block;
        return false;
    }
    else // exit at atomic operation layer
    {
        free(ptr);
        fprintf(stderr, "Error: %s\n", "failed to create memory block");
        return true;
    }
    return false;
}

// Public functions

void register_memory_block_or_exit(void *const ptr)
{
    atomiccally_or_exit(register_memory_block_or_exit_unatomiccaly, ptr);
}

void *safe_malloc_or_exit(const size_t size)
{

    void *const ptr = malloc(size);
    if (ptr != NULL)
    {
        register_memory_block_or_exit(ptr);
        return ptr;
    }
    else
    {
        print_error_and_exit("Failed to allocate memory");
        return NULL;
    }
}
