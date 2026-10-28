#ifndef GANGSTAR_PATH_INDEX_H
#define GANGSTAR_PATH_INDEX_H
#include "gangstar_pak_lookup.h"
#include <stdlib.h>
// Metadata only. Never share a native file or its seek position. Caller holds
// the native filesystem lock while inspecting indexes and search directories.
enum { RIO_PATH_BUCKETS = 4096, RIO_FOLDER_BUCKETS = 1024 };
typedef struct {
    const RioPakEntry *entries; size_t count;
    uint32_t *next, *folder_next; size_t folder_capacity;
    const char *const *folders; size_t folder_count;
    uint64_t folder_stamp; int fold, folder_ready;
    uint32_t heads[RIO_PATH_BUCKETS], folder_heads[RIO_FOLDER_BUCKETS];
    unsigned entry_builds, folder_builds;
} RioPathIndex;
static inline unsigned char rio_path_fold(unsigned char c, int fold) {
    return fold && c >= 'A' && c <= 'Z' ? c + 32 : c;
}
static inline uint32_t rio_path_hash(const char *s, size_t n, int fold) {
    uint32_t h = 2166136261u;
    for (size_t i=0; i<n; ++i) h = (h ^ rio_path_fold((unsigned char)s[i], fold)) * 16777619u;
    return h;
}
static inline const char *rio_path_base(const char *name) {
    const char *p = name;
    for (const char *s=name; *s; ++s) if (*s=='/' || *s=='\\') p=s+1;
    return p;
}
static inline void rio_path_index_clear(RioPathIndex *p) {
    free(p->next); free(p->folder_next); *p=(RioPathIndex){0};
}
static inline int rio_path_index_prepare(RioPathIndex *p, const RioPakEntry *entries,
    size_t count, int fold, const char *const *folders, size_t folder_count, uint64_t stamp) {
    if (count>65536 || folder_count>16384 || (count && !entries) || (folder_count && !folders)) return 0;
    if (p->entries!=entries || p->count!=count || p->fold!=fold || !p->next) {
        uint32_t *next=calloc(count ? count : 1,sizeof(*next));
        if (!next) return 0;
        memset(p->heads,0,sizeof(p->heads));
        for (size_t i=0;i<count;++i) {
            const char *name=entries[i].name;
            if (!name) { free(next); p->entries=NULL; return 0; }
            const char *base=rio_path_base(name);
            unsigned b=rio_path_hash(base,strlen(base),0)%RIO_PATH_BUCKETS;
            next[i]=p->heads[b]; p->heads[b]=(uint32_t)i+1;
        }
        free(p->next); p->next=next; p->entries=entries; p->count=count; p->fold=fold;
        p->folder_ready=0; ++p->entry_builds;
    }
    if (!p->folder_ready || p->folder_stamp!=stamp || p->folders!=folders || p->folder_count!=folder_count) {
        if (folder_count>p->folder_capacity) {
            uint32_t *next=realloc(p->folder_next,folder_count*sizeof(*next));
            if (!next) return 0;
            p->folder_next=next; p->folder_capacity=folder_count;
        }
        memset(p->folder_heads,0,sizeof(p->folder_heads));
        for (size_t i=0;i<folder_count;++i) {
            if (!folders[i]) { p->folder_ready=0; return 0; }
            unsigned b=rio_path_hash(folders[i],strlen(folders[i]),fold)%RIO_FOLDER_BUCKETS;
            p->folder_next[i]=p->folder_heads[b]; p->folder_heads[b]=(uint32_t)i+1;
        }
        p->folders=folders; p->folder_count=folder_count; p->folder_stamp=stamp;
        p->folder_ready=1; ++p->folder_builds;
    }
    return 1;
}
static inline unsigned rio_path_folder_rank(const RioPathIndex *p,const char *prefix,size_t length) {
    unsigned rank=UINT_MAX;
    unsigned b=rio_path_hash(prefix,length,0)%RIO_FOLDER_BUCKETS;
    for (uint32_t node=p->folder_heads[b];node;node=p->folder_next[node-1]) {
        const char *folder=p->folders[node-1];
        size_t i=0;
        while (i<length && folder[i] && rio_path_fold((unsigned char)folder[i],p->fold)==(unsigned char)prefix[i]) ++i;
        if (i==length && !folder[i] && node-1<rank) rank=node-1;
    }
    return rank;
}
// Same first-hit order as native: exact query, then folder/query in folder
// order. INT_MIN means unsupported, so callers must use the original search.
static inline int rio_path_index_find(const RioPathIndex *p,const char *query,int ignore_path) {
    if (!p->next || !p->folder_ready || !query) return INT_MIN;
    int exact=rio_pak_find(p->entries,p->count,query,p->fold,ignore_path);
    if (exact>=0 || exact==INT_MIN) return exact;
    if (ignore_path) return INT_MIN; // Preserve native leading-separator edge cases.
    size_t n=0; char key[256];
    while (n<sizeof(key) && query[n]) { key[n]=(char)rio_path_fold((unsigned char)query[n],p->fold); ++n; }
    if (!n || n==sizeof(key)) return INT_MIN;
    key[n]=0; const char *base=rio_path_base(key);
    unsigned b=rio_path_hash(base,strlen(base),0)%RIO_PATH_BUCKETS;
    unsigned best_rank=UINT_MAX; int best=-1;
    for (uint32_t node=p->heads[b];node;node=p->next[node-1]) {
        unsigned i=node-1; const char *name=p->entries[i].name; size_t length=strlen(name);
        if (length<=n || name[length-n-1]!='/' || strcmp(name+length-n,key)) continue;
        unsigned rank=rio_path_folder_rank(p,name,length-n-1);
        if (rank<best_rank || (rank==best_rank && rank!=UINT_MAX && (best<0 || i<(unsigned)best))) {
            best_rank=rank; best=(int)i;
        }
    }
    return best;
}
#endif
