#include "etc1_mip_reserve.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    // Independent DDS ETC1 mip sizes, including the three smallest block mips.
    const size_t payloads[]={131072,32768,8192,2048,512,128,32,8,8,8};
    size_t end=0;
    for(unsigned level=0;level<10;++level) {
        end+=payloads[level];
        size_t reserved=vgl_etc1_mip_reserve(1,(int)level,512,512,end);
        assert(reserved==(level?174776:131072));
        assert(reserved>=end);
        // Prefix offsets and uploaded level sizes stay identical to old layout.
        assert(end-payloads[level]+payloads[level]<=reserved);
    }
    assert(end==174776);
    assert(vgl_etc1_mip_reserve(0,1,512,512,163840)==163840);
    assert(vgl_etc1_mip_reserve(1,1,1024,1024,655360)==699064);
    assert(vgl_etc1_mip_reserve(1,10,1024,1024,699064)==699064);
    assert(vgl_etc1_mip_reserve(1,1,2048,2048,2621440)==2796216);
    assert(vgl_etc1_mip_reserve(1,11,2048,2048,2796216)==2796216);
    assert(!vgl_etc1_reserve_layout(1,12,2048,2048));
    assert(vgl_etc1_reserve_layout(1,1,256,256));
    assert(vgl_etc1_reserve_layout(1,1,4096,4096));
    assert(vgl_etc1_mip_reserve(1,1,512,256,81920)==87400);
    const size_t rectangle[]={262144,65536,16384,4096,1024,256,64,16,8,8,8};
    size_t rectangle_end=0;
    for(unsigned level=0;level<11;++level) {
        rectangle_end+=rectangle[level];
        size_t reserved=vgl_etc1_mip_reserve(1,(int)level,512,1024,rectangle_end);
        assert(reserved==(level?349544:262144));
        assert(vgl_etc1_mip_reserve(1,(int)level,1024,512,rectangle_end)==reserved);
        assert(reserved>=rectangle_end);
    }
    assert(rectangle_end==349544);
    assert(!vgl_etc1_reserve_layout(1,11,512,1024));
    assert(vgl_etc1_reserve_layout(1,1,512,2048));
    assert(!vgl_etc1_reserve_layout(1,1,3072,3072));
    assert(!vgl_etc1_reserve_layout(1,1,1024,332));
    assert(!vgl_etc1_reserve_layout(1,1,0,512));
    assert(!vgl_etc1_reserve_layout(1,1,8192,8192));
    assert(vgl_etc1_mip_reserve(1,1,4096,4096,10485760)==11184824);
    assert(vgl_etc1_mip_reserve(1,10,512,512,174784)==174784);
    assert(vgl_etc1_mip_reserve(1,-1,512,512,8)==8);
    assert(vgl_etc1_mip_reserve(1,1,512,512,200000)==200000);
    puts("ETC mip reservation: exact complete-chain byte bound, unchanged base-only/other layouts, smallest mips and level guards passed");
}
