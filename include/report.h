#ifndef REPORT_H
#define REPORT_H
#include "cache.h"
void print_configuration(const Cache *cache);
void print_summary(const Cache *cache, const CacheStats *stats);
int write_report(const char *filename, const Cache *cache, const CacheStats *stats, const char *trace_filename);
void print_comparison(const CacheStats *lru, const CacheStats *fifo);
int write_comparison_csv(const char *filename, const CacheStats *lru, const CacheStats *fifo);
#endif
