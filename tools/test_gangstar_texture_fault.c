#include "gangstar_texture_fault.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    RioTextureFault s={0};
    assert(!rio_texture_fault(&s,"mine_lightmap00.tga",1,5,2048,2048));
    s.armed=1;
    assert(!rio_texture_fault(&s,NULL,1,5,2048,2048));
    assert(!rio_texture_fault(&s,"car.tga",1,5,2048,2048));
    assert(!rio_texture_fault(&s,"mine_lightmap00.tga",1,5,512,512));
    assert(!rio_texture_fault(&s,"mine_lightmap00.tga",0,5,2048,2048));
    for(int l=0;l<5;++l)assert(!rio_texture_fault(&s,"mine_lightmap00.tga",1,l,2048,2048));
    assert(s.armed && !s.selected);
    assert(rio_texture_fault(&s,"mine_lightmap00.tga",1,5,2048,2048));
    assert(!s.armed && s.selected==1);
    for(int l=6;l<=11;++l)assert(rio_texture_fault(&s,"mine_lightmap00.tga",1,l,2048,2048));
    assert(!rio_texture_fault(&s,"mine_lightmap00.tga",2,5,2048,2048));
    assert(!rio_texture_fault(&s,"mine_lightmap00.tga",1,12,2048,2048));
    s.selected=0; // The GL slot may be reused after the native scope ends.
    assert(!rio_texture_fault(&s,"mine_lightmap00.tga",1,5,2048,2048));
    puts("Texture fault: opt-in, lightmap/dimension/mip guards, one upload and slot reuse passed");
}
