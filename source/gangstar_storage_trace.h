#ifndef GANGSTAR_STORAGE_TRACE_H
#define GANGSTAR_STORAGE_TRACE_H
#include <stdint.h>
#include <stddef.h>
#ifdef GANGSTAR_STORAGE_TRACE
struct RioStorageSample {uint64_t wall,cpu,locked;unsigned write_seq,write_active,read_seq,read_active;int cpu_ok,thread;};
struct RioStorageSample rio_storage_begin(void);
void rio_storage_locked(struct RioStorageSample *sample);
void rio_storage_end(struct RioStorageSample sample,unsigned kind,uint64_t offset,size_t size,int result);
void rio_storage_write(int active);
void rio_storage_report(void);
#endif
#endif
