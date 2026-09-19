#ifndef REPORT_H
#define REPORT_H
#include "cache.h"
void print_configuration(const Cache *cache);
void print_summary(const Cache *cache, const CacheStats *stats);
int write_report(const char *filename, const Cache *cache, const CacheStats *stats, const char *trace_filename);
#endif
