// hook/malloc_wrap.c
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Memory management types
typedef struct memory_block
{
    void *ptr;
    size_t size;
    struct memory_block *next;
} memory_block_t;

typedef struct
{
    memory_block_t *head;
    pthread_mutex_t mutex;
} memory_manager_t;

/* Prototypes for the real libc allocation functions provided by the linker when
 * using -Wl,--wrap=malloc,--wrap=free, --wrap=calloc, --wrap=realloc.
 */
extern void *__real_malloc(size_t);
extern void __real_free(void *);
extern void *__real_calloc(size_t, size_t);
extern void *__real_realloc(void *, size_t);

static memory_manager_t g_memory_manager = {.head = nullptr, .mutex = PTHREAD_MUTEX_INITIALIZER};

static void safely_exit(const int code)
{
    exit(code);
}

static void print_error_and_exit(const char *const message)
{
    fprintf(stderr, "Error: %s\n", message);
    safely_exit(EXIT_FAILURE);
}

// Automatic initialization using constructor attribute
[[gnu::constructor]] static void init_memory_manager_or_exit(void)
{
    const int result = pthread_mutex_init(&g_memory_manager.mutex, nullptr);
    if (result != 0)
    {
        print_error_and_exit("Failed to initialize mutex");
    }
}

static void atomiccally_or_exit(void (*func)(void *), void *arg)
{
    const int lock_result = pthread_mutex_lock(&g_memory_manager.mutex);
    if (lock_result == 0)
    {
        func(arg);
        const int unlock_result = pthread_mutex_unlock(&g_memory_manager.mutex);
        if (unlock_result != 0)
        {
            print_error_and_exit("kernel mutex unlock error");
        }
    }
    else
    {
        print_error_and_exit("kernel mutex error");
    }
}

// Automatic cleanup using destructor attribute
[[gnu::destructor(101)]] static void cleanup_memory_manager(void)
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
            __real_free(current->ptr);
        }

        // Free the block structure itself
        __real_free(current);

        current = next;
    }
    // Reset the head
    g_memory_manager.head = nullptr;
}

// Pure function to create a new memory block
static memory_block_t *create_memory_block_or_exit(void *const ptr, const size_t size, memory_block_t *const next)
{
    // real malloc the block
    memory_block_t *const block = __real_malloc(sizeof(memory_block_t));
    if (block != nullptr)
    {
        // Initialize the mutable fields before making them immutable conceptually
        block->ptr = ptr;
        block->size = size;
        block->next = next;

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
static void *safe_malloc_or_exit(const size_t size)
{
    if (size > 0)
    {
        // real malloc the content
        void *const ptr = __real_malloc(size);
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

/* replace functions */
void *__wrap_malloc(size_t sz)
{
    void *p = safe_malloc_or_exit(sz);
    fprintf(stderr, "[wrap] malloc(%zu) = %p\n", sz, p);
    return p;
}


static void nullify_in_memory_manager(void *ptr)
{
    memory_block_t *current = g_memory_manager.head;
    while (current != nullptr)
    {
        if (current->ptr == ptr)
        {
            current->ptr = nullptr;
            break;
        }
        current = current->next;
    }
}

void __wrap_free(void *ptr)
{
    fprintf(stderr, "[wrap] free(%p)\n", ptr);

    // Find and nullify the corresponding block's ptr
    atomiccally_or_exit(nullify_in_memory_manager, ptr);
    __real_free(ptr);
}

void *__wrap_calloc(size_t n, size_t sz)
{
    // Check for multiplication overflow
    if (n != 0 && sz > SIZE_MAX / n)
    {
        print_error_and_exit("calloc: multiplication overflow detected");
    }
    void *p = safe_malloc_or_exit(n * sz);
    memset(p, 0, n * sz);
    fprintf(stderr, "[wrap] calloc(%zu, %zu) = %p\n", n, sz, p);
    return p;
}
void *__wrap_realloc(void *ptr, size_t sz)
{
    void *p = safe_malloc_or_exit(sz);
    memcpy(p, ptr, sz);
    __wrap_free(ptr);
    fprintf(stderr, "[wrap] realloc(%p, %zu) = %p\n", ptr, sz, p);
    return p;
}
