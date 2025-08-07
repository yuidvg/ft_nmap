#include "munit.h"
#include "../src/internal.h"
#include <pthread.h>

// Test data structure for thread testing
typedef struct
{
    const int thread_id;
    const size_t allocations_per_thread;
    void **allocated_ptrs;
    const bool *should_continue;
} thread_test_data_t;

// Thread function for concurrent allocation testing
static void *thread_allocate_function(void *const arg)
{
    const thread_test_data_t *const data = (const thread_test_data_t *)arg;

    for (size_t i = 0; i < data->allocations_per_thread && *data->should_continue; ++i)
    {
        const size_t alloc_size = 64 + (i % 256); // Varying sizes
        const void *const ptr = safe_malloc_or_exit(alloc_size);

        if (ptr != nullptr)
        {
            data->allocated_ptrs[i] = (void *)ptr;
            // Write some data to verify the allocation works
            *((int *)ptr) = data->thread_id;
        }
    }

    return nullptr;
}

// Test: Basic memory allocation (constructor/destructor automatic)
static MunitResult test_memory_auto_management(const MunitParameter params[], void *const data)
{
    (void)params;
    (void)data;

    // Should be able to allocate immediately (constructor handles initialization)
    const void *const ptr = safe_malloc_or_exit(64);
    munit_assert_ptr_not_null(ptr);

    // Write test data
    *((int *)ptr) = 42;
    munit_assert_int(*((int *)ptr), ==, 42);

    // Note: destructor will automatically cleanup when program exits
    return MUNIT_OK;
}

// Test: Basic allocation and cleanup
static MunitResult test_basic_allocation(const MunitParameter params[], void *const data)
{
    (void)params;
    (void)data;

    memory_manager_cleanup();

    // Test allocation of various sizes
    const size_t test_sizes[] = {1, 64, 1024, 4096};
    const size_t num_sizes = sizeof(test_sizes) / sizeof(test_sizes[0]);

    for (size_t i = 0; i < num_sizes; ++i)
    {
        const void *const ptr = safe_malloc_or_exit(test_sizes[i]);
        munit_assert_ptr_not_null(ptr);

        // Verify we can write to the allocated memory
        *((char *)ptr) = 'A';
        munit_assert_char(*(char *)ptr, ==, 'A');
    }

    memory_manager_cleanup();
    return MUNIT_OK;
}

// Test: Zero size allocation should fail
static MunitResult test_zero_size_allocation(const MunitParameter params[], void *const data)
{
    (void)params;
    (void)data;

    memory_manager_cleanup();

    const void *const ptr = safe_malloc_or_exit(0);
    munit_assert_ptr_null(ptr);

    memory_manager_cleanup();
    return MUNIT_OK;
}

// Test: Thread safety
static MunitResult test_thread_safety(const MunitParameter params[], void *const data)
{
    (void)params;
    (void)data;

    memory_manager_cleanup();

    const int num_threads = 4;
    const size_t allocations_per_thread = 100;

    pthread_t threads[num_threads];
    thread_test_data_t thread_data[num_threads];
    void *allocated_ptrs[num_threads][allocations_per_thread];
    bool should_continue = true;

    // Initialize thread data
    for (int i = 0; i < num_threads; ++i)
    {
        // Initialize mutable fields before making them conceptually immutable
        *((int *)&thread_data[i].thread_id) = i;
        *((size_t *)&thread_data[i].allocations_per_thread) = allocations_per_thread;
        *((void ***)&thread_data[i].allocated_ptrs) = (void **)allocated_ptrs[i];
        *((const bool **)&thread_data[i].should_continue) = &should_continue;

        // Initialize allocated_ptrs array
        for (size_t j = 0; j < allocations_per_thread; ++j)
        {
            allocated_ptrs[i][j] = nullptr;
        }
    }

    // Create threads
    for (int i = 0; i < num_threads; ++i)
    {
        const int create_result = pthread_create(&threads[i], nullptr,
                                               thread_allocate_function,
                                               &thread_data[i]);
        munit_assert_int(create_result, ==, 0);
    }

    // Wait for threads to complete
    for (int i = 0; i < num_threads; ++i)
    {
        const int join_result = pthread_join(threads[i], nullptr);
        munit_assert_int(join_result, ==, 0);
    }

    // Verify allocations
    for (int i = 0; i < num_threads; ++i)
    {
        for (size_t j = 0; j < allocations_per_thread; ++j)
        {
            if (allocated_ptrs[i][j] != nullptr)
            {
                // Verify the data we wrote
                const int written_value = *((int *)allocated_ptrs[i][j]);
                munit_assert_int(written_value, ==, i);
            }
        }
    }

    memory_manager_cleanup();
    return MUNIT_OK;
}

// Test: Cleanup functionality
static MunitResult test_cleanup(const MunitParameter params[], void *const data)
{
    (void)params;
    (void)data;

    memory_manager_cleanup();

    // Allocate some memory
    const void *const ptr1 = safe_malloc_or_exit(100);
    const void *const ptr2 = safe_malloc_or_exit(200);
    const void *const ptr3 = safe_malloc_or_exit(300);

    munit_assert_ptr_not_null(ptr1);
    munit_assert_ptr_not_null(ptr2);
    munit_assert_ptr_not_null(ptr3);

            // Clean up
    memory_manager_cleanup();

    // Should be able to allocate again after cleanup
    const void *const ptr4 = safe_malloc_or_exit(400);
    munit_assert_ptr_not_null(ptr4);

    memory_manager_cleanup();
    return MUNIT_OK;
}

// Test suite definition
static MunitTest memory_tests[] = {
    {
        .name = "/auto_management",
        .test = test_memory_auto_management,
        .setup = nullptr,
        .tear_down = nullptr,
        .options = MUNIT_TEST_OPTION_NONE,
        .parameters = nullptr
    },
    {
        .name = "/basic_allocation",
        .test = test_basic_allocation,
        .setup = nullptr,
        .tear_down = nullptr,
        .options = MUNIT_TEST_OPTION_NONE,
        .parameters = nullptr
    },
    {
        .name = "/zero_size",
        .test = test_zero_size_allocation,
        .setup = nullptr,
        .tear_down = nullptr,
        .options = MUNIT_TEST_OPTION_NONE,
        .parameters = nullptr
    },
    {
        .name = "/thread_safety",
        .test = test_thread_safety,
        .setup = nullptr,
        .tear_down = nullptr,
        .options = MUNIT_TEST_OPTION_NONE,
        .parameters = nullptr
    },
    {
        .name = "/cleanup",
        .test = test_cleanup,
        .setup = nullptr,
        .tear_down = nullptr,
        .options = MUNIT_TEST_OPTION_NONE,
        .parameters = nullptr
    },
    { nullptr, nullptr, nullptr, nullptr, MUNIT_TEST_OPTION_NONE, nullptr }
};

static const MunitSuite memory_suite = {
    .prefix = "/memory",
    .tests = memory_tests,
    .suites = nullptr,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE
};

int main(const int argc, char *const argv[])
{
    return munit_suite_main(&memory_suite, nullptr, argc, argv);
}