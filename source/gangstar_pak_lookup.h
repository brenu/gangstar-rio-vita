#ifndef GANGSTAR_PAK_LOOKUP_H
#define GANGSTAR_PAK_LOOKUP_H
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <string.h>
typedef struct { uint32_t offset, size; const char *name; uint32_t extra; } RioPakEntry;
#ifdef __vita__
_Static_assert(sizeof(RioPakEntry) == 16, "Rio packed-file index ABI");
#endif
// INT_MIN requests fallback for long names. Native case folding is ASCII-only.
static inline int rio_pak_find(const RioPakEntry *entries, size_t count, const char *name,
    int ignore_case, int ignore_path) {
    if (!name) return -1;
    char normalized[256]; size_t length = 0;
    while (length < sizeof(normalized) && name[length]) ++length;
    if (length == sizeof(normalized) || count > INT_MAX) return INT_MIN;
    const char *key = name;
    if (ignore_case) {
        for (size_t i = 0; i <= length; ++i) {
            unsigned char c = (unsigned char)name[i];
            normalized[i] = c >= 'A' && c <= 'Z' ? c + 32 : c;
        }
        key = normalized;
    }
    if (ignore_path) {
        // Match the native edge case: a separator at position zero is kept.
        for (size_t i = length; i; --i)
            if (key[i - 1] == '/' || key[i - 1] == '\\') {
                if (i > 1) key += i;
                break;
            }
    }
    size_t first = 0, remaining = count;
    while (remaining) {
        size_t half = remaining / 2, middle = first + half;
        if (strcmp(entries[middle].name, key) < 0) { first = middle + 1; remaining -= half + 1; }
        else remaining = half;
    }
    return first < count && !strcmp(entries[first].name, key) ? (int)first : -1;
}
#endif
