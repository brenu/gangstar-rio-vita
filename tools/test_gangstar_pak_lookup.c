#include "gangstar_pak_lookup.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    const RioPakEntry entries[] = {{0, 0, "/kick.wav", 0}, {0, 0, "a.wav", 0},
        {0, 0, "kick.wav", 0}, {0, 0, "kick.wav", 0}, {0, 0, "z.wav", 0}};
    assert(rio_pak_find(entries, 5, "kick.wav", 0, 0) == 2);
    assert(rio_pak_find(entries, 5, "KICK.WAV", 0, 0) == -1);
    assert(rio_pak_find(entries, 5, "KICK.WAV", 1, 0) == 2);
    assert(rio_pak_find(entries, 5, "folder/KICK.WAV", 1, 1) == 2);
    assert(rio_pak_find(entries, 5, "folder\\kick.wav", 0, 1) == 2);
    assert(rio_pak_find(entries, 5, "/kick.wav", 0, 1) == 0);
    assert(rio_pak_find(entries, 5, "folder/", 0, 1) == -1);
    assert(rio_pak_find(entries, 5, "", 1, 1) == -1);
    assert(rio_pak_find(entries, 5, NULL, 1, 1) == -1);
    assert(rio_pak_find(NULL, 0, "x", 0, 0) == -1);
    char long_name[257]; memset(long_name, 'x', 256); long_name[256] = 0;
    assert(rio_pak_find(entries, 5, long_name, 1, 1) == INT_MIN);
    RioPakEntry generated[1000]; char names[1000][32];
    for (unsigned i = 0; i < 1000; ++i) {
        snprintf(names[i], sizeof(names[i]), "file%04u.wav", i);
        generated[i] = (RioPakEntry){i, 1, names[i], 0};
    }
    for (unsigned i = 0; i < 1000; ++i) {
        char query[80]; snprintf(query, sizeof(query), "audio/sub/FILE%04u.WAV", i);
        assert(rio_pak_find(generated, 1000, query, 1, 1) == (int)i);
        assert(rio_pak_find(generated, 1000, query, 1, 0) == -1);
        assert(rio_pak_find(generated, 1000, names[i], 0, 0) == (int)i);
    }
    assert(rio_pak_find(generated, 1000, "file1000.wav", 1, 0) == -1);
    puts("Pak lookup: native ASCII/path rules, first duplicate, misses, bounds and 1000-entry search passed");
}
