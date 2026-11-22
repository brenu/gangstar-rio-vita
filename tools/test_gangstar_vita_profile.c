#include "gangstar_vita_profile.h"
#include <assert.h>
#include <stdio.h>

static void put(unsigned char *p, unsigned offset, int32_t v) { memcpy(p + offset, &v, 4); }
static int32_t get(const unsigned char *p, unsigned offset) { int32_t v; memcpy(&v, p + offset, 4); return v; }
static size_t index_of(const char *key) {
    for (size_t i = 0; i < RIO_PROFILE_FIELD_COUNT; ++i) if (!strcmp(rio_profile_fields[i].key, key)) return i;
    assert(!"unknown key"); return 0;
}
/* Stock DefaultProfile values decoded from PhonePerformanceProfiles.gmap. */
static void stock(unsigned char *p) {
    memset(p, 0xa5, RIO_PROFILE_SIZE);
    put(p, 0x5c, 13000); put(p, 0x60, 60000);
    put(p, 0x8c, 9); put(p, 0x90, 5); put(p, 0x94, 4); put(p, 0x98, 3);
    put(p, 0xb8, 10); put(p, 0xbc, 4);
}
static void parse(RioProfileSettings *s, const char *text) { rio_profile_parse(s, text, strlen(text)); }

int main(void) {
    unsigned char p[RIO_PROFILE_SIZE], reference[RIO_PROFILE_SIZE];
    for (size_t i = 0; i < RIO_PROFILE_FIELD_COUNT; ++i) {
        const RioProfileField *f = &rio_profile_fields[i];
        assert(f->offset % 4 == 0 && f->offset + 4 <= RIO_PROFILE_SIZE && f->min <= f->max);
        assert(f->vita == RIO_PROFILE_KEEP || (f->vita >= f->min && f->vita <= f->max));
    }

    /* Built-in defaults change nothing; the readback reports the stock values. */
    RioProfileSettings s; rio_profile_defaults(&s);
    for (size_t i = 0; i < RIO_PROFILE_FIELD_COUNT; ++i) assert(s.value[i] == RIO_PROFILE_KEEP);
    stock(p); memcpy(reference, p, sizeof(p));
    int before[RIO_PROFILE_FIELD_COUNT], after[RIO_PROFILE_FIELD_COUNT];
    assert(rio_profile_apply(p, &s, before, after) == 0 && !memcmp(p, reference, sizeof(p)));
    assert(before[index_of("farPlaneHighResDist")] == 13000 && after[index_of("farPlaneHighResDist")] == 13000);

    /* Reduced values: only those fields change, nothing else is
       touched, and applying again is idempotent. */
    parse(&s, "farPlaneHighResDist=11000\nnpcFullAnimationDistance=8\nnpcAnimationsPerFrame=2\n");
    assert(!s.errors && rio_profile_apply(p, &s, NULL, NULL) == 3);
    assert(get(p, 0x5c) == 11000 && get(p, 0xb8) == 8 && get(p, 0xbc) == 2);
    assert(get(p, 0x60) == 60000 && get(p, 0x90) == 5 && get(p, 0x98) == 3);
    for (unsigned o = 0; o < RIO_PROFILE_SIZE; ++o)
        if (o < 0x5c || (o >= 0x60 && o < 0xb8) || o >= 0xc0) assert(p[o] == reference[o]);
    assert(rio_profile_apply(p, &s, NULL, NULL) == 0);

    /* A file can tune, clamp, keep stock values, and survive junk lines. */
    rio_profile_defaults(&s);
    parse(&s, "# tuning\r\n  maxSpawnCivilians = 3  # fewer peds\n"
              "farPlaneHighResDist=stock\nnpcAnimationsPerFrame=0\nfarPlaneLowResDist=999999999\n"
              "bogus=4\nmaxSpawnVehicles=x\nnoequals\nmaxSpawn=-5\n\n");
    assert(s.errors == 3 && s.enabled == 1);
    stock(p);
    assert(rio_profile_apply(p, &s, NULL, NULL) == 4);
    assert(get(p, 0x90) == 3 && get(p, 0x5c) == 13000 && get(p, 0xbc) == 1);
    assert(get(p, 0x60) == 200000 && get(p, 0x98) == 3 && get(p, 0x8c) == 1 && get(p, 0xb8) == 10);

    /* enabled=0 restores the stock game behaviour regardless of other lines. */
    rio_profile_defaults(&s); parse(&s, "maxSpawn=2\nenabled=0\n");
    stock(p); memcpy(reference, p, sizeof(p));
    assert(rio_profile_apply(p, &s, NULL, after) == 0 && !memcmp(p, reference, sizeof(p)));
    assert(after[index_of("maxSpawn")] == 9);
    rio_profile_defaults(&s); parse(&s, "enabled=2\nenabled=\n"); assert(s.errors == 2 && s.enabled == 1);

    /* An unexpected layout writes nothing. */
    rio_profile_defaults(&s); stock(p); put(p, 0xbc, 100000); memcpy(reference, p, sizeof(p));
    assert(rio_profile_apply(p, &s, NULL, NULL) == -1 && !memcmp(p, reference, sizeof(p)));

    /* Truncated input (no final newline, embedded NUL-free slice). */
    rio_profile_defaults(&s); parse(&s, "npcFullAnimationDistance=12"); assert(!s.errors);
    stock(p); rio_profile_apply(p, &s, NULL, NULL); assert(get(p, 0xb8) == 12);
    /* The shipped example documents exactly the built-in defaults. */
    FILE *example = fopen("extras/perf.cfg.example", "rb");
    assert(example);
    char text[4096]; size_t n = fread(text, 1, sizeof(text), example); fclose(example);
    RioProfileSettings parsed, defaults; rio_profile_defaults(&parsed); rio_profile_defaults(&defaults);
    rio_profile_parse(&parsed, text, n);
    assert(n < sizeof(text) && !parsed.errors && parsed.enabled);
    for (size_t i = 0; i < RIO_PROFILE_FIELD_COUNT; ++i) assert(parsed.value[i] == defaults.value[i]);
    puts("Vita performance profile: stock defaults, tested values, idempotence, tuning file, clamping, stock/disable, layout guard and example file passed");
}
