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
        const malloc_result_t result = safe_malloc(alloc_size);

        if (result.succeeded && result.ptr != NULL)
        {
            data->allocated_ptrs[i] = result.ptr;
            // Write some data to verify the allocation works
            *((int *)result.ptr) = data->thread_id;
        }
    }

    return NULL;
}

// Test: Memory manager initialization
static MunitResult test_memory_manager_init(const MunitParameter params[], void *const data)
{
    (void)params;
    (void)data;

    // Clean up any previous state
    memory_manager_cleanup();

    // Test initial state
    munit_assert_false(is_memory_manager_initialized());

    // Test initialization
    const malloc_result_t init_result = memory_manager_init();
    munit_assert_true(init_result.succeeded);
    munit_assert_true(is_memory_manager_initialized());

    // Test double initialization (should succeed)
    const malloc_result_t second_init = memory_manager_init();
    munit_assert_true(second_init.succeeded);

    memory_manager_cleanup();
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
        const malloc_result_t result = safe_malloc(test_sizes[i]);
        munit_assert_true(result.succeeded);
        munit_assert_ptr_not_null(result.ptr);

        // Verify we can write to the allocated memory
        *((char *)result.ptr) = 'A';
        munit_assert_char(*(char *)result.ptr, ==, 'A');
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

    const malloc_result_t result = safe_malloc(0);
    munit_assert_false(result.succeeded);
    munit_assert_ptr_null(result.ptr);

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
        *((void ***)&thread_data[i].allocated_ptrs) = allocated_ptrs[i];
        *((const bool **)&thread_data[i].should_continue) = &should_continue;

        // Initialize allocated_ptrs array
        for (size_t j = 0; j < allocations_per_thread; ++j)
        {
            allocated_ptrs[i][j] = NULL;
        }
    }

    // Create threads
    for (int i = 0; i < num_threads; ++i)
    {
        const int create_result = pthread_create(&threads[i], NULL,
                                               thread_allocate_function,
                                               &thread_data[i]);
        munit_assert_int(create_result, ==, 0);
    }

    // Wait for threads to complete
    for (int i = 0; i < num_threads; ++i)
    {
        const int join_result = pthread_join(threads[i], NULL);
        munit_assert_int(join_result, ==, 0);
    }

    // Verify allocations
    for (int i = 0; i < num_threads; ++i)
    {
        for (size_t j = 0; j < allocations_per_thread; ++j)
        {
            if (allocated_ptrs[i][j] != NULL)
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
    const malloc_result_t result1 = safe_malloc(100);
    const malloc_result_t result2 = safe_malloc(200);
    const malloc_result_t result3 = safe_malloc(300);

    munit_assert_true(result1.succeeded);
    munit_assert_true(result2.succeeded);
    munit_assert_true(result3.succeeded);
    munit_assert_true(is_memory_manager_initialized());

    // Clean up
    memory_manager_cleanup();

    // Verify cleanup
    munit_assert_false(is_memory_manager_initialized());

    // Should be able to allocate again after cleanup
    const malloc_result_t result4 = safe_malloc(400);
    munit_assert_true(result4.succeeded);
    munit_assert_ptr_not_null(result4.ptr);

    memory_manager_cleanup();
    return MUNIT_OK;
}

// Test suite definition
static MunitTest memory_tests[] = {
    {
        .name = "/init",
        .test = test_memory_manager_init,
        .setup = NULL,
        .tear_down = NULL,
        .options = MUNIT_TEST_OPTION_NONE,
        .parameters = NULL
    },
    {
        .name = "/basic_allocation",
        .test = test_basic_allocation,
        .setup = NULL,
        .tear_down = NULL,
        .options = MUNIT_TEST_OPTION_NONE,
        .parameters = NULL
    },
    {
        .name = "/zero_size",
        .test = test_zero_size_allocation,
        .setup = NULL,
        .tear_down = NULL,
        .options = MUNIT_TEST_OPTION_NONE,
        .parameters = NULL
    },
    {
        .name = "/thread_safety",
        .test = test_thread_safety,
        .setup = NULL,
        .tear_down = NULL,
        .options = MUNIT_TEST_OPTION_NONE,
        .parameters = NULL
    },
    {
        .name = "/cleanup",
        .test = test_cleanup,
        .setup = NULL,
        .tear_down = NULL,
        .options = MUNIT_TEST_OPTION_NONE,
        .parameters = NULL
    },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

static const MunitSuite memory_suite = {
    .prefix = "/memory",
    .tests = memory_tests,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE
};

int main(const int argc, char *const argv[])
{
    return munit_suite_main(&memory_suite, NULL, argc, argv);
}