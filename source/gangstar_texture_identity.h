#ifndef GANGSTAR_TEXTURE_IDENTITY_H
#define GANGSTAR_TEXTURE_IDENTITY_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#ifndef RIO_TEXTURE_IDENTITIES
#define RIO_TEXTURE_IDENTITIES 512
#endif
typedef struct { uintptr_t object; unsigned serial, frame; char name[192]; } RioTextureIdentity;
typedef struct { RioTextureIdentity slots[RIO_TEXTURE_IDENTITIES]; unsigned serial, collisions; } RioTextureIdentities;
static inline size_t rio_texture_slot(uintptr_t object) { return (object >> 4) % RIO_TEXTURE_IDENTITIES; }
static inline const RioTextureIdentity *rio_texture_identity_find(const RioTextureIdentities *p, uintptr_t object) {
    const RioTextureIdentity *s = &p->slots[rio_texture_slot(object)];
    return object && s->object == object ? s : NULL;
}
static inline int rio_texture_identity_note(RioTextureIdentities *p, uintptr_t object, const char *name, unsigned frame) {
    if (!object || !name) return 0;
    size_t n = 0; while (n < sizeof(p->slots[0].name) && name[n]) ++n;
    if (n == sizeof(p->slots[0].name)) return 0;
    RioTextureIdentity *s = &p->slots[rio_texture_slot(object)];
    if (s->object && s->object != object) ++p->collisions;
    if (!++p->serial) ++p->serial;
    s->object = object; s->serial = p->serial; s->frame = frame;
    memcpy(s->name, name, n + 1); return 1;
}
static inline void rio_texture_identity_forget(RioTextureIdentities *p, uintptr_t object, unsigned serial) {
    RioTextureIdentity *s = &p->slots[rio_texture_slot(object)];
    if (s->object == object && s->serial == serial) s->object = 0;
}
#endif
