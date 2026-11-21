#include "gangstar_loose_index.h"
#include <stdatomic.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
static char indexed_root[512], override_prefix[64];
static size_t root_length, override_length;
static atomic_uint absent_roots, hits, invalidations;
/* Bit 8: names directly inside the root, captured by the startup scan. The
   engine probes "<root><name>_alpha.tga" and "<root>./<name>" for every
   lightmap before reading the OBB; those single names are answered here. */
#define RIO_ROOT_NAMES 64
#define RIO_ROOT_NAME_BYTES 2048
static char root_names[RIO_ROOT_NAME_BYTES];
static unsigned root_name_count;
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
    unsigned mask=3|8;
    size_t used=0;root_name_count=0;
    for(;;) {
        errno=0;
        struct dirent *entry=readdir(dir);
        if(!entry) { if(errno)mask=0;break; }
        if(!strcasecmp(entry->d_name,"_packEtc"))mask&=~1u;
        if(!strcasecmp(entry->d_name,"_packRes"))mask&=~2u;
        size_t length=strlen(entry->d_name)+1;
        if(root_name_count==RIO_ROOT_NAMES || used+length>sizeof(root_names))mask&=~8u;
        else { memcpy(root_names+used,entry->d_name,length);used+=length;++root_name_count; }
    }
    if(closedir(dir))mask=0;
    memcpy(indexed_root,root,n+1);root_length=n;
    atomic_store_explicit(&absent_roots,mask,memory_order_release);
    errno=saved;return mask;
}
unsigned rio_loose_index_init_overrides(const char *prefix,int *probe_error) {
    int saved=errno;
    override_length=0;
    if(probe_error)*probe_error=0;
    size_t n=prefix?strlen(prefix):0;
    if(!n || n>=sizeof(override_prefix) || !separator(prefix[n-1])) { errno=saved;return 0; }
    memcpy(override_prefix,prefix,n+1);
    /* Absent: opendir and stat both fail (whatever errno the platform maps a
       relative miss to). Empty: an existing directory with no entries. */
    int absent=0;
    errno=0;
    DIR *dir=opendir(prefix);
    if(dir) {
        absent=1;
        for(;;) {
            errno=0;
            struct dirent *entry=readdir(dir);
            if(!entry) { if(errno)absent=0;break; }
            if(strcmp(entry->d_name,".") && strcmp(entry->d_name,"..")) { absent=0;break; }
        }
        if(closedir(dir))absent=0;
    } else {
        int error=errno;
        char directory[sizeof(override_prefix)];
        memcpy(directory,prefix,n-1);directory[n-1]=0;
        struct stat status;
        absent=stat(directory,&status)!=0;
        if(probe_error)*probe_error=error?error:-1;
    }
    if(absent) {
        override_length=n;
        atomic_fetch_or_explicit(&absent_roots,4u,memory_order_release);
    }
    errno=saved;return absent?4u:0u;
}
int rio_loose_index_missing(const char *path) {
    unsigned mask=atomic_load_explicit(&absent_roots,memory_order_acquire);
    if(!mask)return 0;
    if((mask&4u) && path && override_length && !strncmp(path,override_prefix,override_length) &&
       !special_components(path+override_length)) {
        atomic_fetch_add_explicit(&hits,1,memory_order_relaxed);
        return 1;
    }
    const char *p=relative(path);
    if(!p)return 0;
    if(mask&8u) {
        const char *name=p;
        while(name[0]=='.' && separator(name[1]))name+=2;
        if(*name && strcmp(name,".") && strcmp(name,"..") && !strchr(name,'/') && !strchr(name,'\\')) {
            const char *known=root_names;
            for(unsigned i=0;i<root_name_count;++i,known+=strlen(known)+1)
                if(!strcasecmp(known,name))return 0;
            atomic_fetch_add_explicit(&hits,1,memory_order_relaxed);
            return 1;
        }
    }
    if(special_components(p))return 0;
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
