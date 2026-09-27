#include "cache.h"
#include "trace.h"
#include "report.h"
#include <stdio.h>
#include <string.h>

static void help(void) {
    puts("CPU Cache Simulator v1.1\nUsage: cpu_cache_simulator --cache-size BYTES --block-size BYTES\n       --associativity WAYS --trace FILE [--policy lru|fifo] [--report FILE]\n       cpu_cache_simulator --sets COUNT --ways WAYS --block-size BYTES\n       [--compare-policies [--csv FILE]] TRACE\nPolicies are lowercase; default lru. Sizes are positive decimal integers.\nComparison uses cold caches and identical input. Create output directories first.\nUse a new output filename. Trace: R/W followed by a hexadecimal address.");
}
static int parse_size(const char *s, size_t *value) {
    size_t n = 0;
    if (!*s) return 0;
    for (; *s; ++s) {
        unsigned int d;
        if (*s < '0' || *s > '9') return 0;
        d = (unsigned int)(*s - '0');
        if (n > (SIZE_MAX - d) / 10) return 0;
        n = n * 10 + d;
    }
    if (!n) return 0;
    *value = n; return 1;
}
/* A disk-backed snapshot gives both policies exactly the same bytes. */
static FILE *snapshot_trace(const char *path) {
    FILE *source = fopen(path, "rb"), *snapshot;
    unsigned char buffer[4096];
    size_t n;
    int ok = 1;
    if (!source) { fprintf(stderr, "Error: cannot open trace file: %s\n", path); return NULL; }
    snapshot = tmpfile();
    if (!snapshot) { fclose(source); fprintf(stderr, "Error: cannot create temporary trace snapshot.\n"); return NULL; }
    while ((n = fread(buffer, 1, sizeof(buffer), source)) != 0) {
        if (fwrite(buffer, 1, n, snapshot) != n) { ok = 0; break; }
    }
    if (ferror(source)) ok = 0;
    if (fclose(source) != 0) ok = 0;
    if (fflush(snapshot) != 0 || fseek(snapshot, 0, SEEK_SET) != 0) ok = 0;
    if (!ok) { fclose(snapshot); fprintf(stderr, "Error: failed to snapshot trace.\n"); return NULL; }
    return snapshot;
}
static int compare(size_t capacity, size_t block, size_t ways, const char *trace, const char *csv) {
    FILE *input = snapshot_trace(trace);
    CacheStats stats[2];
    int run, ok = 1;
    if (!input) return 0;
    printf("Comparison: capacity=%zu sets=%zu ways=%zu block-size=%zu\nCold caches; write-allocate; identical trace snapshot.\n", capacity, capacity / block / ways, ways, block);
    if (ways == 1) puts("Direct mapping: replacement policy cannot affect results.");
    for (run = 0; run < 2; ++run) {
        Cache cache;
        if (fseek(input, 0, SEEK_SET) != 0) { ok = 0; break; }
        if (!cache_init_policy(&cache, capacity, block, ways, run == 0 ? REPLACEMENT_LRU : REPLACEMENT_FIFO)) { ok = 0; break; }
        ok = process_trace_stream(&cache, input, &stats[run], 0, run == 0);
        cache_destroy(&cache);
        if (!ok) break;
    }
    if (fclose(input) != 0) ok = 0;
    if (!ok) { fprintf(stderr, "Error: comparison could not complete.\n"); return 0; }
    print_comparison(&stats[0], &stats[1]);
    return csv ? write_comparison_csv(csv, &stats[0], &stats[1]) : 1;
}
int main(int argc, char **argv) {
    const char *names[] = {"--cache-size", "--block-size", "--associativity", "--trace", "--report", "--policy", "--sets", "--csv"};
    const char *values[8] = {0};
    ReplacementPolicy policy = REPLACEMENT_LRU;
    const char *error;
    size_t capacity = 0, block, ways, sets = 0;
    Cache cache;
    CacheStats stats;
    int i, j, ok, comparison = 0;
    if (argc == 2 && strcmp(argv[1], "--help") == 0) { help(); return 0; }
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--compare-policies") == 0) {
            if (comparison) { fprintf(stderr, "Error: duplicate comparison option.\n"); return 1; }
            comparison = 1; continue;
        }
        if (argv[i][0] && argv[i][0] != '-') j = 3;
        else {
            for (j = 0; j < 8; ++j) if (strcmp(argv[i], names[j]) == 0) break;
            if (strcmp(argv[i], "--ways") == 0) j = 2;
            if (j == 8 || i + 1 == argc || !argv[i + 1][0]) {
                fprintf(stderr, "Error: unknown or incomplete option: %s\n", argv[i]); return 1;
            }
            ++i;
        }
        if (values[j]) { fprintf(stderr, "Error: duplicate option.\n"); return 1; }
        values[j] = argv[i];
    }
    if ((!values[0] && !values[6]) || !values[1] || !values[2] || !values[3]) { fprintf(stderr, "Error: required options are missing or incomplete.\n"); help(); return 1; }
    if (values[0] && values[6]) { fprintf(stderr, "Error: choose --cache-size or --sets, not both.\n"); return 1; }
    if (!parse_size(values[1], &block) || !parse_size(values[2], &ways) || (values[0] && !parse_size(values[0], &capacity)) || (values[6] && !parse_size(values[6], &sets))) {
        fprintf(stderr, "Error: sizes must be positive decimal integers within size_t range.\n"); return 1;
    }
    if (values[6]) {
        if (sets > SIZE_MAX / ways || sets * ways > SIZE_MAX / block) { fprintf(stderr, "Error: cache size overflow.\n"); return 1; }
        capacity = sets * ways * block;
    }
    error = cache_validate(capacity, block, ways);
    if (error) { fprintf(stderr, "Error: %s\n", error); return 1; }
    if (values[5]) {
        if (strcmp(values[5], "fifo") == 0) policy = REPLACEMENT_FIFO;
        else if (strcmp(values[5], "lru") != 0) { fprintf(stderr, "Error: policy must be lowercase lru or fifo.\n"); return 1; }
    }
    if ((comparison && (values[5] || values[4])) || (!comparison && values[7])) { fprintf(stderr, "Error: comparison accepts --csv, not --policy or --report; --csv requires comparison.\n"); return 1; }
    if ((values[4] && strcmp(values[3], values[4]) == 0) || (values[7] && strcmp(values[3], values[7]) == 0)) { fprintf(stderr, "Error: output and trace paths must differ.\n"); return 1; }
    if (comparison) return compare(capacity, block, ways, values[3], values[7]) ? 0 : 1;
    if (!cache_init_policy(&cache, capacity, block, ways, policy)) { fprintf(stderr, "Error: cannot allocate cache metadata.\n"); return 1; }
    print_configuration(&cache);
    ok = process_trace_file(&cache, values[3], &stats);
    if (ok) {
        print_summary(&cache, &stats);
        if (values[4]) ok = write_report(values[4], &cache, &stats, values[3]);
    }
    cache_destroy(&cache);
    return ok ? 0 : 1;
}
