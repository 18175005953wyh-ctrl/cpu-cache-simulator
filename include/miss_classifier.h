#ifndef MISS_CLASSIFIER_H
#define MISS_CLASSIFIER_H
#include "cache.h"

typedef enum { MISS_NONE, MISS_COMPULSORY, MISS_CAPACITY, MISS_CONFLICT } MissType;
typedef struct { uint64_t compulsory, capacity, conflict; } MissBreakdown;
typedef struct {
    Cache shadow_cache;
    uint64_t *seen_blocks;
    unsigned char *occupied;
    size_t table_capacity, block_count;
} MissClassifier;

/* Initialize from a cold LRU cache. Failure leaves a destroyable object. */
int miss_classifier_init(MissClassifier *classifier, const Cache *actual_cache);
/* Insert a block into the set, returning whether it was already present. */
int miss_classifier_record_block(MissClassifier *classifier, uint64_t block, int *seen_before);
/* Call once for EVERY valid access, including actual hits. Failure is fatal. */
int miss_classifier_access(MissClassifier *classifier, uint64_t address, int actual_hit, MissType *type);
void miss_classifier_destroy(MissClassifier *classifier);
#endif
