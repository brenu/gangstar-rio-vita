#include "gangstar_storage_trace.h"
#ifdef GANGSTAR_STORAGE_TRACE
#include "utils/logger.h"
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <pthread.h>
#include <stdatomic.h>
#include <errno.h>
static atomic_uint write_seq,write_active,read_seq,read_active;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static struct Event {unsigned kind;uint64_t offset,wall,cpu,lock_wait;size_t size;int result,cpu_ok,thread;unsigned writes,writer_at_begin,other_reads;} events[64];
static unsigned count,dropped;
static int cpu_time(int thread,uint64_t *value) {
 SceKernelThreadInfo info={.size=sizeof(info)};
 int okay=sceKernelGetThreadInfo(thread,&info)>=0;
 if(okay)*value=info.runClocks;
 return okay;
}
struct RioStorageSample rio_storage_begin(void) {
 int error=errno;
 struct RioStorageSample s={0};
 s.thread=sceKernelGetThreadId();s.cpu_ok=cpu_time(s.thread,&s.cpu);
 s.write_seq=atomic_load(&write_seq);s.write_active=atomic_load(&write_active);
 s.read_active=atomic_fetch_add(&read_active,1);
 s.read_seq=atomic_fetch_add(&read_seq,1)+1;
 s.wall=sceKernelGetProcessTimeWide();s.locked=s.wall;
 errno=error;return s;
}
void rio_storage_locked(struct RioStorageSample *s){int error=errno;s->locked=sceKernelGetProcessTimeWide();errno=error;}
void rio_storage_end(struct RioStorageSample s,unsigned kind,uint64_t offset,size_t size,int result) {
 int error=errno;uint64_t end=sceKernelGetProcessTimeWide();
 unsigned others=s.read_active+atomic_load(&read_seq)-s.read_seq;
 unsigned writes=atomic_load(&write_seq)-s.write_seq;
 atomic_fetch_sub(&read_active,1);
 // Only detailed slow events need an ending thread-info query.
 if(end>=s.wall && end-s.wall>=20000) {
  uint64_t cpu=0;int okay=cpu_time(s.thread,&cpu) && s.cpu_ok && cpu>=s.cpu;
  struct Event e={.kind=kind,.offset=offset,.wall=end-s.wall,.cpu=okay?cpu-s.cpu:0,
   .lock_wait=s.locked-s.wall,.size=size,.result=result,.cpu_ok=okay,.thread=s.thread,
   .writes=writes,.writer_at_begin=s.write_active,.other_reads=others};
  pthread_mutex_lock(&lock);
  if(count<64)events[count++]=e;else ++dropped;
  pthread_mutex_unlock(&lock);
 }
 errno=error;
}
void rio_storage_write(int active) {
 int error=errno;
 if(active)atomic_fetch_add(&write_active,1);else atomic_fetch_sub(&write_active,1);
 atomic_fetch_add(&write_seq,1);errno=error;
}
void rio_storage_report(void) {
 struct Event local[64];unsigned n,loss;int error=errno;
 pthread_mutex_lock(&lock);n=count;loss=dropped;
 for(unsigned i=0;i<n;++i)local[i]=events[i];
 count=dropped=0;pthread_mutex_unlock(&lock);
 for(unsigned i=0;i<n;++i){struct Event *e=&local[i];
  l_info("RIO_STORAGE kind=%u tid=%d offset=%llu bytes=%u result=%d wall_us=%llu cpu_us=%llu cpu_valid=%d lock_wait_us=%llu io_interval_us=%llu log_write_edges=%u writer_at_begin=%u other_reads=%u",
   e->kind,e->thread,(unsigned long long)e->offset,(unsigned)e->size,e->result,(unsigned long long)e->wall,
   (unsigned long long)e->cpu,e->cpu_ok,(unsigned long long)e->lock_wait,
   (unsigned long long)(e->wall-e->lock_wait),e->writes,e->writer_at_begin,e->other_reads);
 }
 if(loss)l_info("RIO_STORAGE dropped=%u",loss);
 errno=error;
}
#endif
