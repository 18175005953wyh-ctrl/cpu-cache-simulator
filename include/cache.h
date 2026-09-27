#ifndef CACHE_H
#define CACHE_H
#include <stddef.h>
#include <stdint.h>

typedef enum { REPLACEMENT_LRU, REPLACEMENT_FIFO } ReplacementPolicy;
typedef struct { int valid; uint64_t tag; uint64_t last_used; uint64_t inserted_at; } CacheLine;
typedef struct {
    size_t cache_size, block_size, associativity, line_count, set_count;
    unsigned int offset_bits, index_bits;
    CacheLine *lines;
    uint64_t clock;
    ReplacementPolicy policy;
} Cache;
typedef struct {
    uint64_t total_accesses, read_accesses, write_accesses, hits, misses, evictions;
} CacheStats;
typedef struct { int hit, eviction; size_t set_index; uint64_t tag; size_t block_offset; } AccessResult;

int is_power_of_two(size_t value);
unsigned int integer_log2(size_t value);
const char *cache_validate(size_t capacity, size_t block, size_t ways);
int cache_init(Cache *cache, size_t capacity, size_t block, size_t ways);
int cache_init_policy(Cache *cache, size_t capacity, size_t block, size_t ways, ReplacementPolicy policy);
const char *cache_policy_name(ReplacementPolicy policy);
void cache_destroy(Cache *cache);
void decode_address(const Cache *cache, uint64_t address, size_t *set, uint64_t *tag, size_t *offset);
/* Requires ways > 0. Selects an empty line first; does not mutate state. */
size_t select_victim(const CacheLine *lines, size_t ways, ReplacementPolicy policy);
/* Requires an initialized cache and clock < UINT64_MAX. */
AccessResult cache_access(Cache *cache, uint64_t address);
#endif
