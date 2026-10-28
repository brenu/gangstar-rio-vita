#include "gangstar_resident_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    RioResidentPolicy p = {0};
    const char *name = "_packetc/southfavela_merged_atlas_exported_atlas0.tga";
    assert(!rio_resident_keep(&p,1,2,name,1024,1024,0,1,16000000));
    assert(!rio_resident_keep(&p,1,2,name,1024,1024,1,0,16000000));
    assert(!rio_resident_keep(&p,1,2,name,1024,1024,1,1,1000));
    assert(!rio_resident_keep(&p,1,2,"huds.bmp",1024,1024,1,1,16000000));
    assert(!rio_resident_keep(&p,1,2,name,512,512,1,1,16000000));
    assert(!p.count && !p.bytes);
    assert(rio_resident_keep(&p,1,2,name,1024,1024,1,1,16000000));
    unsigned bytes = p.bytes;
    assert(bytes == 699064);
    assert(rio_resident_keep(&p,1,2,name,1024,1024,1,1,16000000) && p.bytes == bytes);
    assert(!rio_resident_keep(&p,1,2,name,1024,1024,1,1,0));
    assert(rio_resident_keep(&p,3,2,name,2048,2048,1,1,16000000));
    assert(rio_resident_keep(&p,3,4,name,2048,2048,1,1,16000000));
    assert(!rio_resident_keep(&p,3,5,name,2048,2048,1,1,16000000));
    assert(p.bytes <= RIO_RESIDENT_LIMIT);
    p = (RioResidentPolicy){0};
    for (unsigned i = 0; i < 8; ++i) assert(rio_resident_keep(&p,1,i+1,name,1024,1024,1,1,16000000));
    assert(!rio_resident_keep(&p,1,99,name,1024,1024,1,1,16000000));
    puts("Texture residency: target isolation, ready/bound gating, VRAM reserve, identity deduplication and byte/slot bounds passed");
}
