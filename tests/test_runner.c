#include "cache.h"
#include "trace.h"
#include "report.h"
#include <stdio.h>
#include <string.h>

static int failures = 0, tests = 0;
#define CHECK(name, expression) do { ++tests; if (expression) printf("[PASS] %s\n", name); \
    else { ++failures; printf("[FAIL] %s (line %d)\n", name, __LINE__); } } while (0)

static void configuration_tests(void) {
    Cache c;
    CHECK("power of two", is_power_of_two(1) && is_power_of_two(1024) && !is_power_of_two(0) && !is_power_of_two(6));
    CHECK("integer log2", integer_log2(1) == 0 && integer_log2(16) == 4 && integer_log2(1024) == 10);
    CHECK("invalid capacity", cache_validate(0, 16, 1) && cache_validate(60, 16, 1));
    CHECK("invalid block", cache_validate(64, 0, 1) && cache_validate(60, 3, 1));
    CHECK("invalid associativity", cache_validate(64, 16, 0) && cache_validate(64, 16, 5) && cache_validate(64, 16, 3));
    CHECK("invalid set count", cache_validate(48, 16, 1) != NULL);
    CHECK("allocation limits", cache_validate(SIZE_MAX, 1, SIZE_MAX) && cache_validate(2097152, 1, 1));
    CHECK("non power of two ways allowed", cache_validate(96, 16, 3) == NULL);
    CHECK("failed init is destroyable", !cache_init(&c, 0, 16, 1) && c.lines == NULL);
    cache_destroy(&c);
}
static void mapping_tests(void) {
    Cache c;
    size_t set, offset;
    uint64_t tag;
    AccessResult r;
    if (!cache_init(&c, 64, 16, 1)) { CHECK("init direct mapping", 0); return; }
    decode_address(&c, 0x97, &set, &tag, &offset);
    CHECK("address decomposition", set == 1 && tag == 2 && offset == 7 && c.offset_bits == 4 && c.index_bits == 2);
    decode_address(&c, UINT64_MAX, &set, &tag, &offset);
    CHECK("maximum uint64 address", set == 3 && offset == 15 && tag == UINT64_MAX / 64);
    r = cache_access(&c, 0);
    CHECK("compulsory miss without eviction", !r.hit && !r.eviction);
    CHECK("repeat hit", cache_access(&c, 0).hit);
    CHECK("same block hit", cache_access(&c, 15).hit);
    r = cache_access(&c, 64);
    CHECK("direct mapped eviction", !r.hit && r.eviction);
    CHECK("evicted block misses", !cache_access(&c, 0).hit);
    r = cache_access(&c, 16);
    CHECK("other set is independent", !r.hit && !r.eviction);
    cache_destroy(&c); cache_destroy(&c);
    CHECK("destroy resets state", c.lines == NULL && c.clock == 0);
}
static void associative_tests(void) {
    Cache c;
    AccessResult a, b;
    if (!cache_init(&c, 64, 16, 2)) { CHECK("init associative", 0); return; }
    a = cache_access(&c, 0); b = cache_access(&c, 64);
    CHECK("two ways fill without eviction", !a.hit && !b.hit && !a.eviction && !b.eviction);
    CHECK("two blocks coexist", cache_access(&c, 0).hit && cache_access(&c, 64).hit);
    (void)cache_access(&c, 0);
    a = cache_access(&c, 128);
    CHECK("LRU insertion evicts", !a.hit && a.eviction);
    CHECK("hit refreshes recency", cache_access(&c, 0).hit && !cache_access(&c, 64).hit);
    cache_destroy(&c);
    if (!cache_init(&c, 3, 1, 3)) { CHECK("init fully associative", 0); return; }
    (void)cache_access(&c, 0); (void)cache_access(&c, 1); (void)cache_access(&c, 2);
    (void)cache_access(&c, 0); (void)cache_access(&c, 3);
    CHECK("three way fully associative LRU", c.set_count == 1 && c.index_bits == 0 && cache_access(&c, 2).hit && !cache_access(&c, 1).hit);
    cache_destroy(&c);
}
static void parsing_tests(void) {
    MemoryAccess a;
    const char *bad[] = {"0x10", "X 0x10", "R", "W 0x", "R -1", "R +1", "R 0xgg", "R 1 junk", "R 10000000000000000", "R0x10", "R 1 # trailing", "r 0"};
    size_t i;
    int ok = 1;
    CHECK("trace read hex", parse_trace_line(" R 0XABcd \r\n", &a) == 1 && a.type == ACCESS_READ && a.address == 0xabcd);
    CHECK("trace write maximum", parse_trace_line("W ffffffffffffffff", &a) == 1 && a.type == ACCESS_WRITE && a.address == UINT64_MAX);
    CHECK("blank and comment", parse_trace_line(" \t\r\n", &a) == 0 && parse_trace_line(" # notes", &a) == 0);
    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) if (parse_trace_line(bad[i], &a) != -1) ok = 0;
    CHECK("malformed trace rejection", ok);
}
static void reference_test(void) {
    size_t ways;
    int ok = 1;
    /* Independent model: each set keeps block numbers ordered newest first. */
    for (ways = 1; ways <= 4; ++ways) {
        uint64_t recent[4][4] = {{0}};
        size_t used[4] = {0};
        uint32_t seed = 17;
        Cache c;
        int step;
        if (!cache_init(&c, 4 * ways * 16, 16, ways)) { ok = 0; break; }
        for (step = 0; step < 1000; ++step) {
            uint64_t address, block;
            size_t set, pos, move;
            int hit, eviction;
            AccessResult result;
            seed = seed * UINT32_C(1664525) + UINT32_C(1013904223);
            address = (seed >> 8) % 1024;
            block = address / 16; set = (size_t)(block % 4);
            for (pos = 0; pos < used[set]; ++pos) if (recent[set][pos] == block) break;
            hit = pos < used[set]; eviction = !hit && used[set] == ways;
            if (!hit && used[set] < ways) ++used[set];
            move = hit ? pos : used[set] - 1;
            for (; move > 0; --move) recent[set][move] = recent[set][move - 1];
            recent[set][0] = block;
            result = cache_access(&c, address);
            if (result.hit != hit || result.eviction != eviction) ok = 0;
        }
        cache_destroy(&c);
    }
    CHECK("4000 accesses match independent LRU model", ok);
}
static void file_tests(void) {
    Cache c;
    CacheStats s;
    FILE *f;
    int ok;
    char text[4096];
    size_t n;
    if (!cache_init(&c, 64, 16, 1)) { CHECK("init file tests", 0); return; }
    f = fopen("test-trace.tmp", "wb");
    if (!f) { CHECK("create fixture", 0); cache_destroy(&c); return; }
    fputs("R 0\nW 4\nbad\nW 40\nR 0\n", f); fclose(f);
    ok = process_trace_file(&c, "test-trace.tmp", &s);
    CHECK("read write statistics", ok && s.total_accesses == 4 && s.read_accesses == 2 && s.write_accesses == 2);
    CHECK("statistics expected results", s.hits == 1 && s.misses == 3 && s.evictions == 2);
    CHECK("statistics invariants", s.total_accesses == s.hits + s.misses && s.total_accesses == s.read_accesses + s.write_accesses);
    (void)remove("test-report.tmp");
    ok = write_report("test-report.tmp", &c, &s, "data/test-trace.tmp");
    f = fopen("test-report.tmp", "r"); n = f ? fread(text, 1, sizeof(text) - 1, f) : 0; text[n] = '\0'; if (f) fclose(f);
    CHECK("report content", ok && strstr(text, "25.00%") && strstr(text, "Simulation time:") && strstr(text, "Trace file: test-trace.tmp"));
    CHECK("report refuses overwrite", !write_report("test-report.tmp", &c, &s, "test-trace.tmp"));
    f = fopen("test-trace.tmp", "w"); if (f) fclose(f);
    ok = process_trace_file(&c, "test-trace.tmp", &s);
    print_summary(&c, &s);
    CHECK("empty trace", ok && s.total_accesses == 0);
    f = fopen("test-trace.tmp", "wb");
    if (f) { const char bytes[] = "R 0\0hidden\nW 4\n"; fwrite(bytes, 1, sizeof(bytes) - 1, f); fclose(f); }
    CHECK("embedded NUL record skipped", process_trace_file(&c, "test-trace.tmp", &s) && s.total_accesses == 1 && s.write_accesses == 1);
    c.clock = UINT64_MAX;
    f = fopen("test-trace.tmp", "w"); if (f) { fputs("R 0", f); fclose(f); }
    CHECK("timestamp overflow is stopped", !process_trace_file(&c, "test-trace.tmp", &s) && s.total_accesses == 0);
    cache_destroy(&c); remove("test-trace.tmp"); remove("test-report.tmp");
}
int main(void) {
    configuration_tests(); mapping_tests(); associative_tests(); parsing_tests(); reference_test(); file_tests();
    printf("\n%d tests passed, %d tests failed.\n", tests - failures, failures);
    return failures ? 1 : 0;
}
