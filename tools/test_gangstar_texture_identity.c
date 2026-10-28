#define RIO_TEXTURE_IDENTITIES 4
#include "gangstar_texture_identity.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    RioTextureIdentities p = {0};
    assert(!rio_texture_identity_find(&p, 0));
    assert(rio_texture_identity_note(&p, 16, "first.tga", 5));
    const RioTextureIdentity *s = rio_texture_identity_find(&p,16);
    unsigned old = s->serial; assert(s->frame == 5 && !strcmp(s->name,"first.tga"));
    assert(rio_texture_identity_note(&p, 16, "reused-address.tga", 7));
    rio_texture_identity_forget(&p,16,old);
    s = rio_texture_identity_find(&p,16); assert(s && !strcmp(s->name,"reused-address.tga"));
    assert(rio_texture_identity_note(&p, 80, "collision.tga", 9));
    assert(!rio_texture_identity_find(&p,16) && p.collisions == 1);
    s = rio_texture_identity_find(&p,80); assert(s && s->frame == 9);
    rio_texture_identity_forget(&p,80,s->serial); assert(!rio_texture_identity_find(&p,80));
    char long_name[193]; memset(long_name,'x',192);long_name[192]=0;
    assert(!rio_texture_identity_note(&p,16,long_name,10));
    assert(!rio_texture_identity_note(&p,0,"invalid",10));
    puts("Texture identity: exact addresses, collision isolation, pointer reuse, bounded names and serial-aware invalidation passed");
}
