#ifndef TRACE_H
#define TRACE_H
#include "cache.h"
#include <stdio.h>
typedef enum { ACCESS_READ, ACCESS_WRITE } AccessType;
typedef struct { AccessType type; uint64_t address; } MemoryAccess;
/* 1: valid access, 0: blank/comment, -1: malformed. */
int parse_trace_line(const char *line, MemoryAccess *access);
int process_trace_file(Cache *cache, const char *filename, CacheStats *stats);
/* Caller owns and positions stream. Quiet mode is used for comparisons. */
int process_trace_stream(Cache *cache, FILE *file, CacheStats *stats, int verbose, int warnings);
#endif
