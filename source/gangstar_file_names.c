#include "gangstar_file_names.h"
#include <pthread.h>
#include <string.h>
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static struct { unsigned kind;uintptr_t key;char path[192]; } names[128];
void rio_file_name_set(unsigned kind,uintptr_t key,const char *path) {
    if(!kind || !path)return;
    pthread_mutex_lock(&lock);
    unsigned slot=128;
    for(unsigned i=0;i<128;++i) {
        if(names[i].kind==kind && names[i].key==key){slot=i;break;}
        if(!names[i].kind && slot==128)slot=i;
    }
    if(slot<128) {
        names[slot].key=key;names[slot].kind=kind;
        size_t n=0;while(n<191 && path[n])++n;
        memcpy(names[slot].path,path,n);names[slot].path[n]=0;
    }
    pthread_mutex_unlock(&lock);
}
void rio_file_name_clear(unsigned kind,uintptr_t key) {
    pthread_mutex_lock(&lock);
    for(unsigned i=0;i<128;++i)if(names[i].kind==kind && names[i].key==key) {
        names[i].kind=0;break;
    }
    pthread_mutex_unlock(&lock);
}
void rio_file_name_get(unsigned kind,uintptr_t key,char out[192]) {
    memcpy(out,"<untracked>",12);
    pthread_mutex_lock(&lock);
    for(unsigned i=0;i<128;++i)if(names[i].kind==kind && names[i].key==key) {
        memcpy(out,names[i].path,192);break;
    }
    pthread_mutex_unlock(&lock);
}
