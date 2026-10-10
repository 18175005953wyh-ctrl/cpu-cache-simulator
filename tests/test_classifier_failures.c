/* Compile the same implementation with allocation wrappers only in this test
   executable, so real allocation failures are deterministic and reproducible. */
#include <stdlib.h>
#include <stdio.h>
static int allocations, fail_at;
static void *test_malloc(size_t bytes) {
    if (++allocations == fail_at) return NULL;
    return malloc(bytes);
}
static void *test_calloc(size_t count, size_t size) {
    if (++allocations == fail_at) return NULL;
    return calloc(count, size);
}
#define malloc test_malloc
#define calloc test_calloc
#include "../src/cache.c"
#include "../src/miss_classifier.c"
#undef malloc
#undef calloc
#include "trace.h"

int main(void) {
    Cache actual;
    MissClassifier c;
    int failure, tests = 0, failures = 0;
    if (!cache_init(&actual, 2, 1, 1)) return 1;
    for (failure = 1; failure <= 3; ++failure) {
        allocations = 0; fail_at = failure;
        ++tests;
        if (miss_classifier_init(&c, &actual) || c.shadow_cache.lines || c.seen_blocks || c.occupied)
            ++failures;
        miss_classifier_destroy(&c);
    }
    for (failure = 1; failure <= 2; ++failure) {
        int seen, ok = 1;
        uint64_t i;
        fail_at = 0;
        if (!miss_classifier_init(&c, &actual)) { ++failures; continue; }
        for (i = 0; i < 11; ++i) if (!miss_classifier_record_block(&c, i, &seen)) ok = 0;
        allocations = 0; fail_at = failure;
        ++tests;
        if (miss_classifier_record_block(&c, 11, &seen) || c.block_count != 11 || c.table_capacity != 16) ok = 0;
        fail_at = 0;
        for (i = 0; i < 11; ++i) if (!miss_classifier_record_block(&c, i, &seen) || !seen) ok = 0;
        if (!miss_classifier_record_block(&c, 11, &seen) || seen || c.block_count != 12) ok = 0;
        if (!ok) ++failures;
        miss_classifier_destroy(&c);
    }
    {
        FILE *file = tmpfile();
        CacheStats stats;
        MissBreakdown breakdown;
        int i;
        fail_at = 0;
        if (!file || !miss_classifier_init(&c, &actual)) {
            if (file) fclose(file);
            cache_destroy(&actual); return 1;
        }
        for (i = 0; i < 16; ++i) fprintf(file, "R %x\n", (unsigned int)i);
        rewind(file); allocations = 0; fail_at = 1;
        ++tests;
        if (process_trace_stream_ex(&actual, file, &stats, &c, &breakdown, 0, 0) ||
            stats.total_accesses != 11 || breakdown.compulsory != 11 || c.block_count != 11)
            ++failures;
        fclose(file); miss_classifier_destroy(&c);
    }
    cache_destroy(&actual);
    printf("%d allocation failure tests passed, %d failed.\n", tests - failures, failures);
    return failures ? 1 : 0;
}
