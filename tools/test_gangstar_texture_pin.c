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
    // Car damage maps: reduced 1024 chain only, one instance per name, within budget.
    assert(!rio_texture_pin_add(&p,90,"_packetc/car_cops_texture_damage_d.tga",2048,2048,2796344,RIO_TEXTURE_PIN_LIMIT,retain,&refs));
    assert(!rio_texture_pin_add(&p,90,"_packetc/car_cops_texture_damage_d.tga",512,512,174904,RIO_TEXTURE_PIN_LIMIT,retain,&refs));
    assert(!rio_texture_pin_add(&p,90,"car_cops_texture_d.tga",1024,1024,699192,RIO_TEXTURE_PIN_LIMIT,retain,&refs));
    for (unsigned i=0; i<RIO_TEXTURE_PIN_CARS; ++i)
        assert(rio_texture_pin_add(&p,100+i,rio_texture_pin_car_names[i],1024,1024,699192,RIO_TEXTURE_PIN_LIMIT,retain,&refs)==1);
    assert(p.count==20 && refs==20 && p.bytes==2796416+4*699064 && p.bytes<=RIO_TEXTURE_PIN_LIMIT);
    assert(rio_texture_pin_find(&p,"./car_cops_texture_damage_d.tga")==100);
    assert(rio_texture_pin_find(&p,"_packetc/car_poor_texture_damage_d_alpha.tga")==103);
    assert(!rio_texture_pin_add(&p,200,"./car_cops_texture_damage_d.tga",1024,1024,699192,RIO_TEXTURE_PIN_LIMIT,retain,&refs));
    assert(rio_texture_pin_key_matches("./car_cops_texture_damage_d.tga","_packetc/car_cops_texture_damage_d.tga"));
    while (rio_texture_pin_pop(&p)) --refs;
    assert(!p.count && !p.bytes && !refs);
    puts("Texture pins: exact targets, 512 gating, one ref per name/object, byte bound and release bookkeeping passed");
}
