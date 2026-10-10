#include "report.h"
#include <inttypes.h>
#include <stdio.h>
#include <time.h>

static void configuration(FILE *out, const Cache *c) {
    fprintf(out, "Replacement policy: %s\n", cache_policy_name(c->policy));
    if (c->associativity == 1) fprintf(out, "Direct mapped: replacement policy does not affect results.\n");
    fprintf(out, "Cache size:        %zu bytes\nBlock size:        %zu bytes\nAssociativity:     %zu\nCache lines:       %zu\nNumber of sets:    %zu\nOffset bits:       %u\nIndex bits:        %u\n",
        c->cache_size, c->block_size, c->associativity, c->line_count, c->set_count, c->offset_bits, c->index_bits);
}
static void summary(FILE *out, const Cache *c, const CacheStats *s) {
    fprintf(out, "\n===== Cache Simulation Summary =====\n");
    configuration(out, c);
    fprintf(out, "Total accesses:    %" PRIu64 "\nRead accesses:     %" PRIu64 "\nWrite accesses:    %" PRIu64 "\nHits:              %" PRIu64 "\nMisses:            %" PRIu64 "\nEvictions:         %" PRIu64 "\n",
        s->total_accesses, s->read_accesses, s->write_accesses, s->hits, s->misses, s->evictions);
    if (s->total_accesses) fprintf(out, "Hit rate:          %.2f%%\nMiss rate:         %.2f%%\n",
        100.0 * (double)s->hits / (double)s->total_accesses, 100.0 * (double)s->misses / (double)s->total_accesses);
    else fprintf(out, "No valid accesses.\nHit rate:          N/A\nMiss rate:         N/A\n");
}
void print_configuration(const Cache *cache) { configuration(stdout, cache); }
void print_summary(const Cache *cache, const CacheStats *stats) { summary(stdout, cache, stats); }
static void miss_breakdown(FILE *out, const MissBreakdown *b, const CacheStats *s) {
    const char *names[] = {"Compulsory misses:", "Capacity misses:", "Conflict misses:"};
    uint64_t counts[] = {b->compulsory, b->capacity, b->conflict};
    size_t i;
    fputs("\n===== 3C Miss Classification =====\n", out);
    for (i = 0; i < 3; ++i) {
        fprintf(out, "%-18s %" PRIu64, names[i], counts[i]);
        if (s->misses) fprintf(out, " (%.2f%% of misses)", 100.0 * (double)counts[i] / (double)s->misses);
        fputc('\n', out);
    }
    fprintf(out, "Classified misses: %" PRIu64 "\n", b->compulsory + b->capacity + b->conflict);
    if (!s->misses) fputs("Miss breakdown:    N/A\n", out);
    fputs("Classification model: seen-block set + same-capacity fully-associative LRU shadow cache\n", out);
}
void print_miss_breakdown(const MissBreakdown *b, const CacheStats *s) { miss_breakdown(stdout, b, s); }
static void comparison_row(FILE *out, const char *policy, const CacheStats *s, int csv) {
    if (csv) {
        fprintf(out, "%s,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",", policy, s->total_accesses, s->hits, s->misses, s->evictions);
        if (s->total_accesses) fprintf(out, "%.9f", (double)s->hits / (double)s->total_accesses);
        fputc('\n', out);
    } else {
        fprintf(out, "%-6s  %8" PRIu64 "  %8" PRIu64 "  %8" PRIu64 "  %9" PRIu64 "  ", policy, s->total_accesses, s->hits, s->misses, s->evictions);
        if (s->total_accesses) fprintf(out, "%7.2f%%\n", 100.0 * (double)s->hits / (double)s->total_accesses);
        else fprintf(out, "     N/A\n");
    }
}
void print_comparison(const CacheStats *lru, const CacheStats *fifo) {
    puts("\nPolicy  Accesses      Hits    Misses  Evictions  Hit Rate");
    comparison_row(stdout, "LRU", lru, 0);
    comparison_row(stdout, "FIFO", fifo, 0);
}
int write_comparison_csv(const char *filename, const CacheStats *lru, const CacheStats *fifo) {
    FILE *out = fopen(filename, "wx");
    int ok;
    if (!out) { fprintf(stderr, "Error: cannot create CSV; use a new filename and existing parent directory.\n"); return 0; }
    fputs("policy,accesses,hits,misses,evictions,hit_rate\n", out);
    comparison_row(out, "LRU", lru, 1);
    comparison_row(out, "FIFO", fifo, 1);
    ok = !ferror(out);
    if (fclose(out) != 0) ok = 0;
    if (!ok) fprintf(stderr, "Error: failed to write complete CSV.\n");
    return ok;
}
int write_report(const char *filename, const Cache *cache, const CacheStats *stats, const char *trace_filename) {
    return write_report_ex(filename, cache, stats, trace_filename, NULL);
}
int write_report_ex(const char *filename, const Cache *cache, const CacheStats *stats,
    const char *trace_filename, const MissBreakdown *breakdown) {
    FILE *file = fopen(filename, "wx");
    time_t now = time(NULL);
    struct tm *utc = gmtime(&now);
    char stamp[64] = "unavailable";
    const char *base = trace_filename, *p;
    int ok;
    if (!file) { fprintf(stderr, "Error: cannot create report. Use a new filename and check the parent directory and permissions.\n"); return 0; }
    for (p = trace_filename; *p; ++p) if (*p == '/' || *p == '\\') base = p + 1;
    if (utc) (void)strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S UTC", utc);
    fprintf(file, "CPU Cache Simulator\nSimulation time: %s\nTrace file: %s\nPolicy: %s, write allocate; no data or dirty bits\n", stamp, base, cache_policy_name(cache->policy));
    summary(file, cache, stats);
    if (breakdown) miss_breakdown(file, breakdown, stats);
    ok = !ferror(file);
    if (fclose(file) != 0) ok = 0;
    if (!ok) fprintf(stderr, "Error: failed to write complete report.\n");
    return ok;
}
