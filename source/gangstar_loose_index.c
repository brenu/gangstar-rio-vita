#include "gangstar_loose_index.h"
#include <stdatomic.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <errno.h>
static char indexed_root[512];
static size_t root_length;
static atomic_uint absent_roots, hits, invalidations;
static int separator(char c) { return c=='/' || c=='\\'; }
static int special_components(const char *path) {
    while(*path) {
        const char *start=path;
        while(*path && !separator(*path))++path;
        size_t n=(size_t)(path-start);
        if((n==1 && start[0]=='.') || (n==2 && start[0]=='.' && start[1]=='.'))return 1;
        if(*path)++path;
    }
    return 0;
}
static const char *relative(const char *path) {
    return path && root_length && !strncasecmp(path,indexed_root,root_length)
        ? path+root_length : NULL;
}
static int component(const char *path,const char *name) {
    size_t n=strlen(name);
    return !strncasecmp(path,name,n) && (!path[n] || separator(path[n]));
}
unsigned rio_loose_index_init(const char *root) {
    int saved=errno;
    atomic_store(&absent_roots,0);atomic_store(&hits,0);atomic_store(&invalidations,0);
    root_length=0;
    if(!root) { errno=saved;return 0; }
    size_t n=strlen(root);
    if(!n || n>=sizeof(indexed_root) || root[n-1]!='/') { errno=saved;return 0; }
    DIR *dir=opendir(root);
    if(!dir) { errno=saved;return 0; }
    unsigned mask=3;
    for(;;) {
        errno=0;
        struct dirent *entry=readdir(dir);
        if(!entry) { if(errno)mask=0;break; }
        if(!strcasecmp(entry->d_name,"_packEtc"))mask&=~1u;
        if(!strcasecmp(entry->d_name,"_packRes"))mask&=~2u;
    }
    if(closedir(dir))mask=0;
    memcpy(indexed_root,root,n+1);root_length=n;
    atomic_store_explicit(&absent_roots,mask,memory_order_release);
    errno=saved;return mask;
}
int rio_loose_index_missing(const char *path) {
    unsigned mask=atomic_load_explicit(&absent_roots,memory_order_acquire);
    if(!mask)return 0;
    const char *p=relative(path);
    if(!p || special_components(p))return 0;
    unsigned bit=component(p,"_packEtc")?1u:component(p,"_packRes")?2u:0;
    if(!(mask&bit))return 0;
    atomic_fetch_add_explicit(&hits,1,memory_order_relaxed);
    return 1;
}
void rio_loose_index_mutation(const char *path) {
    if(!atomic_load_explicit(&absent_roots,memory_order_relaxed))return;
    const char *p=relative(path);
    /* Save activity cannot create a pack root. Anything less certain disables
       the snapshot before the operation, even when that operation later fails. */
    if(p && !special_components(p) && component(p,"saves"))return;
    if(atomic_exchange_explicit(&absent_roots,0,memory_order_acq_rel))
        atomic_fetch_add_explicit(&invalidations,1,memory_order_relaxed);
}
RioLooseIndexStats rio_loose_index_stats(void) {
    return (RioLooseIndexStats){atomic_load(&absent_roots),atomic_exchange(&hits,0),
        atomic_exchange(&invalidations,0)};
}
