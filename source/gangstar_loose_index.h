#ifndef GANGSTAR_LOOSE_INDEX_H
#define GANGSTAR_LOOSE_INDEX_H
#include <stdint.h>
/* Startup-only scan. Bit 1: _packEtc absent, bit 2: _packRes absent,
   bit 8: the root's own entry names are known (single-name probes answered).
   Existing entries (files or directories), incomplete scans and errors stay live. */
unsigned rio_loose_index_init(const char *root);
/* Optional relative override directory (Android "../Documents/"): the engine
   probes it before the OBB for every lightmap _alpha companion. When the
   directory is absent or empty at startup, bit 4 answers those probes without
   storage access until any mutation. Call after rio_loose_index_init.
   probe_error (optional) receives opendir's errno when it failed. */
unsigned rio_loose_index_init_overrides(const char *prefix,int *probe_error);
int rio_loose_index_missing(const char *path);
void rio_loose_index_mutation(const char *path);
typedef struct { unsigned absent, hits, invalidations; } RioLooseIndexStats;
RioLooseIndexStats rio_loose_index_stats(void);
#endif
