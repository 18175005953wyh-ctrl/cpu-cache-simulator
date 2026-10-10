#include "trace.h"
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

int parse_trace_line(const char *line, MemoryAccess *access) {
    const unsigned char *p = (const unsigned char *)line;
    uint64_t value = 0;
    unsigned int digit;
    AccessType type;
    int digits = 0;
    while (isspace(*p)) ++p;
    if (!*p || *p == '#') return 0;
    if (*p != 'R' && *p != 'W') return -1;
    type = *p++ == 'R' ? ACCESS_READ : ACCESS_WRITE;
    if (!isspace(*p)) return -1;
    while (isspace(*p)) ++p;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
    while (isxdigit(*p)) {
        digit = *p <= '9' ? (unsigned int)(*p - '0') : (unsigned int)(tolower(*p) - 'a' + 10);
        if (value > (UINT64_MAX - digit) / 16) return -1;
        value = value * 16 + digit; ++p; digits = 1;
    }
    if (!digits) return -1;
    while (isspace(*p)) ++p;
    if (*p) return -1;
    access->type = type; access->address = value;
    return 1;
}
int process_trace_file(Cache *cache, const char *filename, CacheStats *stats) {
    return process_trace_file_ex(cache, filename, stats, NULL, NULL);
}
int process_trace_file_ex(Cache *cache, const char *filename, CacheStats *stats,
    MissClassifier *classifier, MissBreakdown *breakdown) {
    FILE *file = fopen(filename, "rb");
    int ok;
    memset(stats, 0, sizeof(*stats));
    if (breakdown) memset(breakdown, 0, sizeof(*breakdown));
    if (!file) { fprintf(stderr, "Error: cannot open trace file: %s\n", filename); return 0; }
    ok = process_trace_stream_ex(cache, file, stats, classifier, breakdown, 1, 1);
    if (fclose(file) != 0) { fprintf(stderr, "Error: failed to close trace file.\n"); ok = 0; }
    return ok;
}
int process_trace_stream(Cache *cache, FILE *file, CacheStats *stats, int verbose, int warnings) {
    return process_trace_stream_ex(cache, file, stats, NULL, NULL, verbose, warnings);
}
int process_trace_stream_ex(Cache *cache, FILE *file, CacheStats *stats,
    MissClassifier *classifier, MissBreakdown *breakdown, int verbose, int warnings) {
    char line[1024];
    uint64_t line_number = 0;
    int ch, ok = 1;
    memset(stats, 0, sizeof(*stats));
    if (breakdown) memset(breakdown, 0, sizeof(*breakdown));
    if ((classifier == NULL) != (breakdown == NULL) || (classifier &&
        (cache->policy != REPLACEMENT_LRU || cache->clock || classifier->shadow_cache.clock ||
         classifier->block_count || !classifier->shadow_cache.lines ||
         classifier->shadow_cache.policy != REPLACEMENT_LRU ||
         classifier->shadow_cache.set_count != 1 ||
         classifier->shadow_cache.cache_size != cache->cache_size ||
         classifier->shadow_cache.block_size != cache->block_size))) {
        fprintf(stderr, "Error: classification requires matching cold LRU caches and a breakdown.\n");
        return 0;
    }
    /* Read bytes into a bounded line buffer, so embedded NUL cannot hide suffixes. */
    while ((ch = fgetc(file)) != EOF) {
        size_t length = 0;
        int invalid = 0, parsed;
        MemoryAccess access = {ACCESS_READ, 0};
        AccessResult result;
        ++line_number;
        do {
            if (ch == '\n') break;
            if (ch == 0) invalid = 1;
            if (length < sizeof(line) - 1) line[length++] = (char)ch;
            else invalid = 1;
        } while ((ch = fgetc(file)) != EOF);
        line[length] = '\0';
        parsed = invalid ? -1 : parse_trace_line(line, &access);
        if (parsed < 0) { if (warnings) fprintf(stderr, "Warning: line %" PRIu64 ": invalid or overlong trace record; skipped.\n", line_number); continue; }
        if (!parsed) continue;
        if (cache->clock == UINT64_MAX || stats->total_accesses == UINT64_MAX) {
            fprintf(stderr, "Error: access counter limit reached.\n"); ok = 0; break;
        }
        result = cache_access(cache, access.address);
        if (classifier) {
            MissType type;
            if (!miss_classifier_access(classifier, access.address, result.hit, &type)) {
                fprintf(stderr, "Error: 3C classification failed (allocation or counter limit); simulation aborted.\n");
                ok = 0; break;
            }
            if (type == MISS_COMPULSORY) ++breakdown->compulsory;
            else if (type == MISS_CAPACITY) ++breakdown->capacity;
            else if (type == MISS_CONFLICT) ++breakdown->conflict;
        }
        ++stats->total_accesses;
        if (access.type == ACCESS_READ) ++stats->read_accesses; else ++stats->write_accesses;
        if (result.hit) ++stats->hits; else ++stats->misses;
        if (result.eviction) ++stats->evictions;
        if (verbose) printf("#%" PRIu64 " %c 0x%016" PRIx64 " -> %s%s | set=%zu tag=0x%" PRIx64 " offset=%zu\n",
            stats->total_accesses, access.type == ACCESS_READ ? 'R' : 'W', access.address,
            result.hit ? "HIT" : "MISS", result.eviction ? " EVICTION" : "",
            result.set_index, result.tag, result.block_offset);
    }
    if (ferror(file)) { fprintf(stderr, "Error: failed to read trace file.\n"); ok = 0; }
    return ok;
}
