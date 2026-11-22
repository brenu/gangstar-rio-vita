#ifndef GANGSTAR_VITA_PROFILE_H
#define GANGSTAR_VITA_PROFILE_H
/* Vita overrides for the game's own phone performance profile (gPhonePerf).
 *
 * The Vita matches none of the 29 profiles in the patch OBB's
 * _packRes/PhonePerformanceProfiles.gmap, so the game runs DefaultProfile.
 * DefaultProfile is already a low tier (no post effects, no HD shaders), but
 * the Vita's 444 MHz Cortex-A9 is slower per core than the weakest phones the
 * table was tuned for, and the frame thread is CPU-bound at 28-43 ms.
 *
 * Built-in defaults change nothing: on hardware (22 Nov 2026) the engine used
 * these values (farPlaneHighResDist 11000, NPC budget 2 / 8 m) but frame time
 * did not change measurably, so the stock look is kept. perf.cfg can still set
 * any field for experiments.
 *
 * Every field below is read live by the engine from the selected profile, so
 * changing it once, right after profile selection, is enough. Offsets are from
 * Structs::PerfProfile::Read in the supplied 1.2.1g library (element size
 * 0x124); the engine's own PerfTweak/RoundRobinTweaks debug menus register the
 * same addresses under the names used here.
 *
 * Portable, allocation-free and host-testable: no Vita headers. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#define RIO_PROFILE_SIZE 0x124u
#define RIO_PROFILE_KEEP INT_MIN

typedef struct {
    const char *key;
    unsigned offset;       /* int32 field inside Structs::PerfProfile */
    int min, max;          /* plausible stock range and accepted override range */
    int vita;              /* built-in override, or RIO_PROFILE_KEEP (all are KEEP) */
} RioProfileField;

/* Stock DefaultProfile values are in the comments. */
static const RioProfileField rio_profile_fields[] = {
    /* High-resolution city draw/stream distance. 11000 is Gameloft's own value
       for every "Low" profile (MedScrLow1-3, Scr540Low, SmallScr, ...). The
       low-resolution city still draws out to farPlaneLowResDist. Stock 13000. */
    {"farPlaneHighResDist",      0x5c, 2000, 100000, RIO_PROFILE_KEEP},
    /* Low-resolution city distance. Stock 60000 in every profile. */
    {"farPlaneLowResDist",       0x60, 5000, 200000, RIO_PROFILE_KEEP},
    /* Ambient population. Stock 9 / 5 / 4 / 3 (all profiles use these). */
    {"maxSpawn",                 0x8c, 1, 100, RIO_PROFILE_KEEP},
    {"maxSpawnCivilians",        0x90, 1, 100, RIO_PROFILE_KEEP},
    {"maxSpawnParkedVehicles",   0x94, 1, 100, RIO_PROFILE_KEEP},
    {"maxSpawnVehicles",         0x98, 1, 100, RIO_PROFILE_KEEP},
    /* NPCRoundRobinManager: NPCs nearer than this (metres) animate every frame;
       farther ones with a skippable animation share a per-frame budget.
       Stock 10 m and 4 per frame in every profile. */
    {"npcFullAnimationDistance", 0xb8, 0, 100, RIO_PROFILE_KEEP},
    {"npcAnimationsPerFrame",    0xbc, 1, 64, RIO_PROFILE_KEEP},
};
#define RIO_PROFILE_FIELD_COUNT (sizeof(rio_profile_fields) / sizeof(rio_profile_fields[0]))

typedef struct {
    int enabled;
    int value[RIO_PROFILE_FIELD_COUNT];  /* RIO_PROFILE_KEEP leaves the stock value */
    unsigned errors;                     /* malformed or unknown lines */
} RioProfileSettings;

static inline void rio_profile_defaults(RioProfileSettings *s) {
    s->enabled = 1; s->errors = 0;
    for (size_t i = 0; i < RIO_PROFILE_FIELD_COUNT; ++i) s->value[i] = rio_profile_fields[i].vita;
}

static inline int rio_profile_space(char c) { return c == ' ' || c == '\t' || c == '\r'; }

/* Parses an optional key=value file over the defaults. '#' starts a comment.
 * A value of "stock" keeps the game's own value; "enabled=0" disables every
 * override. Out-of-range numbers are clamped; anything else counts as an error
 * and leaves that setting unchanged. */
static inline void rio_profile_parse(RioProfileSettings *s, const char *text, size_t length) {
    size_t at = 0;
    while (at < length) {
        size_t end = at;
        while (end < length && text[end] != '\n') ++end;
        size_t a = at, b = end;
        for (size_t i = a; i < b; ++i) if (text[i] == '#') { b = i; break; }
        while (a < b && rio_profile_space(text[a])) ++a;
        while (b > a && rio_profile_space(text[b - 1])) --b;
        at = end + 1;
        if (a == b) continue;
        size_t eq = a;
        while (eq < b && text[eq] != '=') ++eq;
        if (eq == b) { ++s->errors; continue; }
        size_t ke = eq, vs = eq + 1;
        while (ke > a && rio_profile_space(text[ke - 1])) --ke;
        while (vs < b && rio_profile_space(text[vs])) ++vs;
        size_t key_length = ke - a, value_length = b - vs;
        const char *key = text + a, *value = text + vs;
        int keep = value_length == 5 && !memcmp(value, "stock", 5);
        long number = 0; int valid = value_length > 0 && value_length <= 9;
        size_t p = 0;
        if (valid && value[0] == '-') { if (value_length == 1) valid = 0; p = 1; }
        for (; valid && p < value_length; ++p) {
            if (value[p] < '0' || value[p] > '9') { valid = 0; break; }
            number = number * 10 + (value[p] - '0');
        }
        if (valid && value[0] == '-') number = -number;
        if (key_length == 7 && !memcmp(key, "enabled", 7)) {
            if (valid && (number == 0 || number == 1)) s->enabled = (int)number;
            else ++s->errors;
            continue;
        }
        size_t field = RIO_PROFILE_FIELD_COUNT;
        for (size_t i = 0; i < RIO_PROFILE_FIELD_COUNT; ++i)
            if (strlen(rio_profile_fields[i].key) == key_length &&
                !memcmp(rio_profile_fields[i].key, key, key_length)) { field = i; break; }
        if (field == RIO_PROFILE_FIELD_COUNT || (!keep && !valid)) { ++s->errors; continue; }
        if (keep) { s->value[field] = RIO_PROFILE_KEEP; continue; }
        const RioProfileField *f = &rio_profile_fields[field];
        if (number < f->min) number = f->min;
        if (number > f->max) number = f->max;
        s->value[field] = (int)number;
    }
}

/* Writes the selected overrides. Every stock value is range-checked first; if
 * any looks wrong the layout is not the verified one and nothing is written.
 * Returns the number of changed fields, or -1 on a layout mismatch. Optional
 * before[]/after[] receive the stock and resulting values. */
static inline int rio_profile_apply(unsigned char *profile, const RioProfileSettings *s,
    int before[RIO_PROFILE_FIELD_COUNT], int after[RIO_PROFILE_FIELD_COUNT]) {
    int stock[RIO_PROFILE_FIELD_COUNT];
    for (size_t i = 0; i < RIO_PROFILE_FIELD_COUNT; ++i) {
        const RioProfileField *f = &rio_profile_fields[i];
        int32_t v; memcpy(&v, profile + f->offset, sizeof(v));
        if (v < f->min || v > f->max) return -1;
        stock[i] = v;
        if (before) before[i] = v;
    }
    int changed = 0;
    for (size_t i = 0; i < RIO_PROFILE_FIELD_COUNT; ++i) {
        int32_t v = stock[i];
        if (s->enabled && s->value[i] != RIO_PROFILE_KEEP && s->value[i] != v) {
            v = s->value[i];
            memcpy(profile + rio_profile_fields[i].offset, &v, sizeof(v));
            ++changed;
        }
        if (after) after[i] = v;
    }
    return changed;
}
#endif
