#!/usr/bin/env python3
"""Exercise the actual diagnostic ledger, including collision churn."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
source=(root/'source/dynlib.c').read_text()
block=source[source.index('#define RIO_HEAP_TRACK_MIN'):source.index('void gangstar_heap_diag_report(')]
test=r"""
#include <assert.h>
int main(void) {
    void *p[100];
    for(unsigned i=0;i<100;++i) {
        p[i]=(void *)(uintptr_t)(0x100000u+i*262144u);
        rio_heap_allocated(p[i],8192,123);
    }
    assert(rio_heap_track_count==100 && rio_heap_track_bytes==819200);
    for(unsigned i=0;i<100;i+=2)rio_heap_freed(p[i]);
    for(unsigned i=1;i<100;i+=2)assert(rio_heap_find(p[i])<RIO_HEAP_TRACK_SLOTS);
    for(unsigned i=1;i<100;i+=2)rio_heap_freed(p[i]);
    assert(!rio_heap_track_count && !rio_heap_track_bytes);
    for(unsigned n=0;n<50000;++n) {
        void *p=(void *)(uintptr_t)(0x100000u+n*16u);
        rio_heap_allocated(p,8192,123);rio_heap_freed(p);
    }
    for(unsigned i=0;i<RIO_HEAP_TRACK_SLOTS;++i)assert(!rio_heap_tracks[i].ptr);
    for(unsigned i=0;i<RIO_HEAP_TRACK_SLOTS;++i)
        rio_heap_allocated((void *)(uintptr_t)(0x100000u+i*16u),8192,123);
    assert(rio_heap_track_count==RIO_HEAP_TRACK_SLOTS);
    for(unsigned i=0;i<RIO_HEAP_TRACK_SLOTS;++i)
        rio_heap_freed((void *)(uintptr_t)(0x100000u+i*16u));
    assert(!rio_heap_track_count && !rio_heap_track_bytes);
#ifdef GANGSTAR_HEAP_TRACE
    unsigned selected=0;void *sample=NULL;
    for(unsigned i=1;i<1048576;++i) {
        void *p=(void *)(uintptr_t)((uint32_t)i*4096u);
        if(rio_heap_sample(p)){++selected;sample=p;}
    }
    assert(selected>800 && selected<1300 && sample);
    rio_heap_allocated(sample,16,999);
    assert(rio_heap_track_count==1 && rio_heap_track_bytes==16*1024);
    rio_heap_freed(sample);assert(!rio_heap_track_count && !rio_heap_track_bytes);
#endif
    return 0;
}
"""
out=root/'work/tests/heap_ledger.c'
out.write_text('#include <stddef.h>\n#include <stdint.h>\n#include <stdatomic.h>\n#define malloc_usable_size(ptr) 8192u\n'+block+test)
exe=out.with_suffix('')
for mode in [[],['-DGANGSTAR_HEAP_TRACE']]:
    subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',*mode,str(out),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=20)
print('Heap ledger: collisions, full capacity, 50000 churn cycles and unbiased aligned-pointer sampling passed')
