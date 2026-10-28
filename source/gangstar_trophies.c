#include "gangstar_trophies.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <pthread.h>
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
// GL2JNILib.u has 100 entries. Android persists decimal flags, default 127.
int gangstar_trophy_notify(const char *path, int index) {
    if (index < 0 || index >= 100) return 0;
    pthread_mutex_lock(&lock);
    int flags[100], result = -1;
    for (unsigned i = 0; i < 100; ++i) flags[i] = 127;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[64]; unsigned i = 0; int valid = 1;
        while (fgets(line, sizeof(line), f)) {
            char *end; errno = 0;
            long value = strtol(line, &end, 10);
            if (i == 100 || end == line || errno || value < INT_MIN || value > INT_MAX ||
                (*end && strcmp(end, "\n") && strcmp(end, "\r\n"))) { valid = 0; break; }
            flags[i++] = (int)value;
        }
        if (ferror(f)) valid = 0;
        fclose(f);
        if (!valid) goto done;
    } else if (errno != ENOENT) goto done;
    flags[index] = 1;
    size_t length = strlen(path);
    char *temporary = malloc(length + 5);
    if (!temporary) goto done;
    memcpy(temporary, path, length); memcpy(temporary + length, ".tmp", 5);
    f = fopen(temporary, "w");
    if (f) {
        int ok = 1;
        for (unsigned i = 0; i < 100; ++i) if (fprintf(f, "%d\n", flags[i]) < 0) ok = 0;
        if (fclose(f)) ok = 0;
        if (ok && !rename(temporary, path)) result = 0;
        else remove(temporary);
    }
    free(temporary);
done:
    pthread_mutex_unlock(&lock);
    return result;
}
