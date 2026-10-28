#ifndef GANGSTAR_DYNLINK_H
#define GANGSTAR_DYNLINK_H
/* -1: unknown, 0: native/platform veneer, 1: already loaded core. */
int gangstar_library_kind(const char *path);
void *gangstar_dlopen(const char *path, int flags);
void *gangstar_dlsym(void *handle, const char *symbol);
int gangstar_dlclose(void *handle);
char *gangstar_dlerror(void);
void *gangstar_builtin_symbol(const char *name);
#endif
