#ifndef GANGSTAR_LOOSE_INDEX_H
#define GANGSTAR_LOOSE_INDEX_H
#include <stdint.h>
/* Startup-only scan. Bit 1: _packEtc absent, bit 2: _packRes absent.
   Existing entries (files or directories), incomplete scans and errors stay live. */
unsigned rio_loose_index_init(const char *root);
int rio_loose_index_missing(const char *path);
void rio_loose_index_mutation(const char *path);
typedef struct { unsigned absent, hits, invalidations; } RioLooseIndexStats;
RioLooseIndexStats rio_loose_index_stats(void);
#endif
