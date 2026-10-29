#include "gangstar_archive_cache.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static const char *archive="ux0:/data/gangstarrio/main.12030.com.gameloft.android.ANMP.GloftG4HM.obb";
static char path[]="/tmp/rio-shared-XXXXXX";
static unsigned physical_opens;
static void *memory;
int __real_open(const char *,int,...);
int __wrap_open(const char *name,int flags,...) {
 assert(!strcmp(name,archive) && flags==O_RDONLY);++physical_opens;
 return __real_open(path,flags);
}
int sceKernelAllocMemBlock(const char *n,int t,size_t size,void *o){(void)n;(void)t;(void)o;memory=calloc(1,size);return memory?1:-1;}
int sceKernelGetMemBlockBase(int id,void **out){(void)id;*out=memory;return 0;}
int sceKernelFreeMemBlock(int id){(void)id;free(memory);return 0;}
void _log_print(int t,const char *fmt,...){(void)t;(void)fmt;}
static void *reader(void *argument) {
 int fd=*(int *)argument;
 for(unsigned i=0;i<100;++i) {
  unsigned value=0;
  assert(gangstar_archive_seek(fd,4096*i,SEEK_SET)==4096*i);
  assert(gangstar_archive_read(fd,&value,sizeof(value))==sizeof(value));
  assert(value==i+100);
 }
 return NULL;
}
int main(void) {
 int seed=mkstemp(path);assert(seed>=0);assert(!ftruncate(seed,2119380688));
 for(unsigned i=0;i<100;++i){unsigned value=i+100;assert(pwrite(seed,&value,sizeof(value),4096*i)==sizeof(value));}
 close(seed);gangstar_archive_init();
 enum {COUNT=96};FILE *streams[COUNT];int fds[COUNT];
 assert(gangstar_archive_open_shared("other")==-2);
 for(unsigned i=0;i<COUNT;++i) {
  fds[i]=gangstar_archive_open_shared(archive);assert(fds[i]>=0);
  streams[i]=gangstar_archive_stream(fdopen(fds[i],"rb"),archive,"rb");assert(streams[i]);
  assert(!setvbuf(streams[i],NULL,_IONBF,0));
  unsigned value=0;assert(!fseek(streams[i],4096*i,SEEK_SET));
  assert(fread(&value,1,sizeof(value),streams[i])==sizeof(value) && value==i+100);
  assert(ftell(streams[i])==(long)(4096*i+sizeof(value)));
  assert(gangstar_archive_fileno(streams[i])==fds[i]);
 }
 assert(physical_opens==1);
 pthread_t threads[8];
 for(unsigned i=0;i<8;++i)assert(!pthread_create(&threads[i],NULL,reader,&fds[i]));
 for(unsigned i=0;i<8;++i)assert(!pthread_join(threads[i],NULL));
 for(unsigned i=0;i<COUNT;i+=2)assert(!fclose(streams[i]));
 for(unsigned i=1;i<COUNT;i+=2) {
  unsigned value=0;assert(!fseek(streams[i],0,SEEK_SET));
  assert(fread(&value,1,sizeof(value),streams[i])==sizeof(value) && value==100);
  assert(!fclose(streams[i]));
 }
 int fd=gangstar_archive_open_shared(archive);assert(fd>=0 && physical_opens==1);
 assert(gangstar_archive_seek(fd,-4,SEEK_END)==2119380684);
 assert(gangstar_archive_seek(fd,-1,SEEK_SET)==-1 && errno==EINVAL);
 assert(!gangstar_archive_close(fd));unlink(path);free(memory);
 puts("Shared OBB: 96 independent readers, one physical open, concurrent seeks/reads, exposure and sibling close isolation passed");
}
