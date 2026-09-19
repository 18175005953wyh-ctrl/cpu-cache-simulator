#include "cache.h"
#include "trace.h"
#include "report.h"
#include <stdio.h>
#include <string.h>

static void help(void) {
    puts("CPU Cache Simulator\nUsage: cpu_cache_simulator --cache-size BYTES --block-size BYTES\n       --associativity WAYS --trace FILE [--report FILE]\n       cpu_cache_simulator --help\nUse positive decimal sizes. Trace: R/W followed by a hexadecimal address.\nCreate the report parent directory before running. Use a new report filename.");
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
int main(int argc, char **argv) {
    const char *names[] = {"--cache-size", "--block-size", "--associativity", "--trace", "--report"};
    const char *values[5] = {0};
    const char *error;
    size_t capacity, block, ways;
    Cache cache;
    CacheStats stats;
    int i, j, ok;
    if (argc == 2 && strcmp(argv[1], "--help") == 0) { help(); return 0; }
    for (i = 1; i < argc; i += 2) {
        for (j = 0; j < 5; ++j) if (strcmp(argv[i], names[j]) == 0) break;
        if (j == 5 || i + 1 == argc || values[j] || !argv[i + 1][0]) {
            fprintf(stderr, "Error: unknown, duplicate or incomplete option: %s\n", argv[i]); help(); return 1;
        }
        values[j] = argv[i + 1];
    }
    if (!values[0] || !values[1] || !values[2] || !values[3]) { fprintf(stderr, "Error: four required options are missing or incomplete.\n"); help(); return 1; }
    if (!parse_size(values[0], &capacity) || !parse_size(values[1], &block) || !parse_size(values[2], &ways)) {
        fprintf(stderr, "Error: sizes and associativity must be positive decimal integers within size_t range.\n"); return 1;
    }
    error = cache_validate(capacity, block, ways);
    if (error) { fprintf(stderr, "Error: %s\n", error); return 1; }
    if (values[4] && strcmp(values[3], values[4]) == 0) { fprintf(stderr, "Error: report and trace paths must differ.\n"); return 1; }
    if (!cache_init(&cache, capacity, block, ways)) { fprintf(stderr, "Error: cannot allocate cache metadata.\n"); return 1; }
    print_configuration(&cache);
    ok = process_trace_file(&cache, values[3], &stats);
    if (ok) {
        print_summary(&cache, &stats);
        if (values[4]) ok = write_report(values[4], &cache, &stats, values[3]);
    }
    cache_destroy(&cache);
    return ok ? 0 : 1;
}
