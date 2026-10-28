#include "gangstar_texture_pin.h"
#include <assert.h>
#include <stdio.h>
static unsigned refs;
static void retain(void *context, uintptr_t object) { assert(context == &refs && object); ++refs; }
int main(void) {
    RioTexturePins p = {0};
    const char *a = "_packetc/southfavela_merged_atlas_exported_atlas0.tga";
    const char *b = "southfavela_merged_atlas_exported_atlas0_alpha.tga";
    assert(!rio_texture_pin_add(&p,1,"huds.bmp",512,512,174904,RIO_TEXTURE_PIN_LIMIT,retain,&refs));
    assert(!rio_texture_pin_add(&p,1,a,1024,1024,699192,RIO_TEXTURE_PIN_LIMIT,retain,&refs));
    assert(rio_texture_pin_add(&p,1,a,512,512,174904,RIO_TEXTURE_PIN_LIMIT,retain,&refs)==1);
    assert(refs==1 && p.count==1 && p.bytes==174776);
    assert(rio_texture_pin_find(&p,a)==1);
    assert(rio_texture_pin_key_matches(a,NULL));
    assert(rio_texture_pin_key_matches(a,"./southfavela_merged_atlas_exported_atlas0.tga"));
    assert(rio_texture_pin_find(&p,"./southfavela_merged_atlas_exported_atlas0.tga")==1);
    assert(!rio_texture_pin_key_matches(a,"../southfavela_merged_atlas_exported_atlas0.tga"));
    assert(!rio_texture_pin_key_matches(a,"./other/southfavela_merged_atlas_exported_atlas0.tga"));
    assert(!rio_texture_pin_key_matches(a,"././southfavela_merged_atlas_exported_atlas0.tga"));
    assert(rio_texture_pin_key_matches(a,a));
    assert(rio_texture_pin_key_matches(a,"southfavela_merged_atlas_exported_atlas0.tga"));
    assert(!rio_texture_pin_key_matches(a,b));
    assert(!rio_texture_pin_key_matches(a,"_packetc/southfavela_merged_atlas_exported_atlas0.tgaA"));
    assert(!rio_texture_pin_key_matches(a,"other/southfavela_merged_atlas_exported_atlas0.tga"));
    assert(rio_texture_pin_find(&p,"southfavela_merged_atlas_exported_atlas0.tga")==1);
    assert(!rio_texture_pin_find(&p,"other/southfavela_merged_atlas_exported_atlas0.tga"));
    assert(!rio_texture_pin_find(&p,"_packetc/other/southfavela_merged_atlas_exported_atlas0.tga"));
    assert(!rio_texture_pin_find(&p,NULL));
    assert(!rio_texture_pin_find(&p,b));
    assert(rio_texture_pin_add(&p,1,a,512,512,174904,RIO_TEXTURE_PIN_LIMIT,retain,&refs)==2 && refs==1);
    assert(!rio_texture_pin_add(&p,2,a,512,512,174904,RIO_TEXTURE_PIN_LIMIT,retain,&refs) && refs==1);
    assert(!rio_texture_pin_add(&p,2,b,512,512,174904,300000,retain,&refs));
    assert(rio_texture_pin_add(&p,2,b,512,512,174904,RIO_TEXTURE_PIN_LIMIT,retain,&refs)==1 && refs==2);
    assert(rio_texture_pin_pop(&p)==2 && p.count==1 && p.bytes==174776); --refs;
    assert(rio_texture_pin_pop(&p)==1 && !p.count && !p.bytes); --refs;
    assert(!rio_texture_pin_pop(&p) && !refs);
    assert(!rio_texture_pin_find(&p,a));
    for (unsigned i=0; i<RIO_TEXTURE_PIN_TARGETS; ++i)
        assert(rio_texture_pin_add(&p,i+1,rio_texture_pin_names[i],512,512,174904,RIO_TEXTURE_PIN_LIMIT,retain,&refs)==1);
    assert(p.count==16 && p.bytes==2796416 && refs==16);
    assert(!rio_texture_pin_name("southfavela_lightmap00.tga"));
    while (rio_texture_pin_pop(&p)) --refs;
    assert(!p.count && !p.bytes && !refs);
    puts("Texture pins: exact targets, 512 gating, one ref per name/object, byte bound and release bookkeeping passed");
}
