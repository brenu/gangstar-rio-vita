"""Exercise the actual temporary-pool branch used by selective snapshots."""
from pathlib import Path
import subprocess,os
s=Path('lib/vitagl/source/vgl.c').read_text()
a=s.index('uint8_t *vgl_reserve_data_pool(uint32_t size) {')
b=s.index('\n}\n#endif',a)+2
body=s[a:b]+'\n'
prefix=r"""
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#define RIO_SELECTIVE_VERTEX_COPY 1
static uint8_t *circular_data_pool_ptr[3],*circular_data_pool_limit[3];
static int vgl_circular_idx;
static unsigned fallback_calls,dirty_calls;static void *fallback;
static void *gpu_alloc_mapped_for_cpu(size_t n){assert(n && !(n&15));++fallback_calls;fallback=aligned_alloc(16,n);return fallback;}
static void mark_as_dirty(void *p){assert(p==fallback);++dirty_calls;}
"""
main=r"""
int main(void){
 _Alignas(16) uint8_t pool[4096];
 circular_data_pool_ptr[0]=pool;circular_data_pool_limit[0]=pool+sizeof(pool);
 unsigned sizes[]={1,3,6,12,13,20,37,81,255};
 uintptr_t previous=(uintptr_t)pool;
 for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);++i){
  uint8_t *p=vgl_reserve_data_pool(sizes[i]);assert(p && !((uintptr_t)p&15));
  assert((uintptr_t)p>=previous);for(unsigned j=0;j<sizes[i];++j)p[j]=(uint8_t)i;
  previous=(uintptr_t)p+sizes[i];assert(!((uintptr_t)circular_data_pool_ptr[0]&15));
 }
 assert(!fallback_calls);
 // Alignment also repairs a cursor left unaligned by another producer.
 circular_data_pool_ptr[0]=pool+1;assert(vgl_reserve_data_pool(3)==pool+16);
 circular_data_pool_ptr[0]=pool+sizeof(pool)-16;
 assert(vgl_reserve_data_pool(16)==pool+sizeof(pool)-16);
 assert(vgl_reserve_data_pool(1)==fallback && fallback_calls==1 && dirty_calls==1);
 assert(circular_data_pool_ptr[0]==pool+sizeof(pool));free(fallback);
 assert(!vgl_reserve_data_pool(UINT32_MAX) && fallback_calls==1);
 vgl_circular_idx=1;circular_data_pool_ptr[1]=pool;circular_data_pool_limit[1]=pool+sizeof(pool);
 assert(vgl_reserve_data_pool(7)==pool && circular_data_pool_ptr[0]==pool+sizeof(pool));
}
"""
p=Path('work/tests/vertex_pool_alignment.c');p.write_text(prefix+body+main)
for sanitize in (False,True):
 cmd=['cc','-std=c11','-O1' if sanitize else '-O2','-Wall','-Wextra','-Werror']
 if sanitize:cmd+=['-fsanitize=address,undefined']
 subprocess.run(cmd+[str(p),'-o','work/tests/vertex_pool_alignment'],check=True)
 subprocess.run(['work/tests/vertex_pool_alignment'],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('Vertex pool alignment: actual allocator preserves 16-byte alignment after odd tails, disjoint reservations, exact capacity, aligned fallback, overflow rejection and rotating pool isolation (ASan/UBSan)')
