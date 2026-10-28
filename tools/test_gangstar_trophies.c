#include "gangstar_trophies.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
static void check(const char *path) {
    FILE *f = fopen(path, "r"); assert(f);
    for (int i = 0; i < 100; ++i) {
        int flag; assert(fscanf(f, "%d", &flag) == 1);
        assert(flag == (i == 0 || i == 99 ? 1 : 127));
    }
    int extra; assert(fscanf(f, "%d", &extra) == EOF); fclose(f);
}
int main(void) {
    char directory[] = "/tmp/rio-trophy-XXXXXX"; assert(mkdtemp(directory));
    char path[256]; snprintf(path, sizeof(path), "%s/androidTrophy.dat", directory);
    assert(!gangstar_trophy_notify(path, -1) && !gangstar_trophy_notify(path, 100));
    assert(access(path, F_OK));
    assert(!gangstar_trophy_notify(path, 0));
    assert(!gangstar_trophy_notify(path, 99)); check(path);
    assert(!gangstar_trophy_notify(path, 0)); check(path);
    FILE *f = fopen(path, "w"); assert(f); fputs("invalid\n", f); fclose(f);
    assert(gangstar_trophy_notify(path, 3) == -1);
    f = fopen(path, "r"); assert(f && fgetc(f) == 'i'); fclose(f);
    assert(!remove(path) && !rmdir(directory));
    puts("Android trophy flags: fresh install, bounds, 100-entry format, preservation, repeats and corrupt-file protection passed");
}
