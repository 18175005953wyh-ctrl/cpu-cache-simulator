#include "miss_classifier.h"
#include <stdlib.h>
#include <string.h>

static size_t slot_for(uint64_t key, const uint64_t *keys, const unsigned char *used, size_t capacity) {
    uint64_t hash = key;
    size_t slot;
    hash ^= hash >> 30; hash *= UINT64_C(0xbf58476d1ce4e5b9);
    hash ^= hash >> 27; hash *= UINT64_C(0x94d049bb133111eb);
    hash ^= hash >> 31;
    slot = (size_t)hash & (capacity - 1);
    while (used[slot] && keys[slot] != key) slot = (slot + 1) & (capacity - 1);
    return slot;
}
static int resize_set(MissClassifier *c, size_t capacity) {
    uint64_t *keys;
    unsigned char *used;
    size_t i;
    if (capacity < 16 || !is_power_of_two(capacity) || capacity > SIZE_MAX / sizeof(*keys)) return 0;
    keys = malloc(capacity * sizeof(*keys));
    used = calloc(capacity, sizeof(*used));
    if (!keys || !used) { free(keys); free(used); return 0; }
    for (i = 0; i < c->table_capacity; ++i) if (c->occupied[i]) {
        size_t slot = slot_for(c->seen_blocks[i], keys, used, capacity);
        keys[slot] = c->seen_blocks[i]; used[slot] = 1;
    }
    free(c->seen_blocks); free(c->occupied);
    c->seen_blocks = keys; c->occupied = used; c->table_capacity = capacity;
    return 1;
}
int miss_classifier_record_block(MissClassifier *c, uint64_t block, int *seen_before) {
    size_t slot, limit;
    if (!c || !seen_before || !c->table_capacity || !c->occupied || !c->seen_blocks) return 0;
    slot = slot_for(block, c->seen_blocks, c->occupied, c->table_capacity);
    *seen_before = c->occupied[slot] != 0;
    if (*seen_before) return 1;
    /* floor(capacity * 0.7), without overflowing size_t. */
    limit = (c->table_capacity / 10) * 7 + (c->table_capacity % 10) * 7 / 10;
    if (c->block_count >= limit) {
        if (c->table_capacity > SIZE_MAX / 2 || !resize_set(c, c->table_capacity * 2)) return 0;
        slot = slot_for(block, c->seen_blocks, c->occupied, c->table_capacity);
    }
    c->seen_blocks[slot] = block; c->occupied[slot] = 1; ++c->block_count;
    return 1;
}
void miss_classifier_destroy(MissClassifier *c) {
    if (!c) return;
    cache_destroy(&c->shadow_cache);
    free(c->seen_blocks); free(c->occupied); memset(c, 0, sizeof(*c));
}
int miss_classifier_init(MissClassifier *c, const Cache *actual) {
    if (!c) return 0;
    memset(c, 0, sizeof(*c));
    if (!actual || !actual->lines || actual->clock || actual->policy != REPLACEMENT_LRU) return 0;
    if (!cache_init_policy(&c->shadow_cache, actual->cache_size, actual->block_size,
                           actual->line_count, REPLACEMENT_LRU) || !resize_set(c, 16)) {
        miss_classifier_destroy(c); return 0;
    }
    return 1;
}
int miss_classifier_access(MissClassifier *c, uint64_t address, int actual_hit, MissType *type) {
    int seen;
    AccessResult shadow;
    if (!type) return 0;
    *type = MISS_NONE;
    if (!c || !c->shadow_cache.lines || c->shadow_cache.clock == UINT64_MAX) return 0;
    if (!miss_classifier_record_block(c, address / c->shadow_cache.block_size, &seen)) return 0;
    shadow = cache_access(&c->shadow_cache, address);
    if (!actual_hit) *type = !seen ? MISS_COMPULSORY : (shadow.hit ? MISS_CONFLICT : MISS_CAPACITY);
    return 1;
}
