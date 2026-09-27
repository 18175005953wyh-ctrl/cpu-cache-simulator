#include "cache.h"
#include <stdlib.h>
#include <string.h>

int is_power_of_two(size_t value) { return value != 0 && (value & (value - 1)) == 0; }
unsigned int integer_log2(size_t value) {
    unsigned int bits = 0;
    while (value > 1) { value >>= 1; ++bits; }
    return bits;
}
const char *cache_validate(size_t capacity, size_t block, size_t ways) {
    size_t lines, sets;
    if (!capacity) return "Cache size must be positive.";
    if (!block) return "Block size must be positive.";
    if (!ways) return "Associativity must be positive.";
    if (!is_power_of_two(block)) return "Block size must be a power of two.";
    if (capacity % block) return "Cache size must be divisible by block size.";
    lines = capacity / block;
    if (ways > lines) return "Associativity cannot exceed the number of lines.";
    if (lines % ways) return "Line count must be divisible by associativity.";
    sets = lines / ways;
    if (!is_power_of_two(sets)) return "Number of sets must be a positive power of two.";
    if (lines > SIZE_MAX / sizeof(CacheLine)) return "Cache metadata allocation is too large.";
    /* A teaching simulator: avoid exhausting memory on accidental huge inputs. */
    if (lines > 1048576) return "At most 1048576 cache lines are supported.";
    return NULL;
}
int cache_init(Cache *cache, size_t capacity, size_t block, size_t ways) {
    memset(cache, 0, sizeof(*cache));
    if (cache_validate(capacity, block, ways)) return 0;
    cache->lines = calloc(capacity / block, sizeof(*cache->lines));
    if (!cache->lines) return 0;
    cache->cache_size = capacity; cache->block_size = block; cache->associativity = ways;
    cache->line_count = capacity / block; cache->set_count = cache->line_count / ways;
    cache->offset_bits = integer_log2(block); cache->index_bits = integer_log2(cache->set_count);
    return 1;
}
void cache_destroy(Cache *cache) { free(cache->lines); memset(cache, 0, sizeof(*cache)); }
void decode_address(const Cache *cache, uint64_t address, size_t *set, uint64_t *tag, size_t *offset) {
    uint64_t block = address / cache->block_size;
    *set = (size_t)(block % cache->set_count);
    *tag = block / cache->set_count;
    *offset = (size_t)(address % cache->block_size);
}
size_t select_victim(const CacheLine *lines, size_t ways) {
    size_t i, oldest = 0;
    for (i = 0; i < ways; ++i) {
        if (!lines[i].valid) return i;
        /* The smallest timestamp is the least recently accessed line. */
        if (lines[i].last_used < lines[oldest].last_used) oldest = i;
    }
    return oldest;
}
AccessResult cache_access(Cache *cache, uint64_t address) {
    AccessResult result = {0};
    CacheLine *lines;
    size_t i;
    decode_address(cache, address, &result.set_index, &result.tag, &result.block_offset);
    lines = cache->lines + result.set_index * cache->associativity;
    ++cache->clock;
    for (i = 0; i < cache->associativity; ++i) {
        if (lines[i].valid && lines[i].tag == result.tag) {
            result.hit = 1; lines[i].last_used = cache->clock; return result;
        }
    }
    i = select_victim(lines, cache->associativity);
    result.eviction = lines[i].valid;
    lines[i].valid = 1; lines[i].tag = result.tag; lines[i].last_used = cache->clock;
    return result;
}
