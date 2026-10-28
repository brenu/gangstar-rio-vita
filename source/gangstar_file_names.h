#ifndef GANGSTAR_FILE_NAMES_H
#define GANGSTAR_FILE_NAMES_H
#include <stdint.h>
/* Bounded diagnostic names, independent namespaces for FILE pointers and fds. */
void rio_file_name_set(unsigned kind,uintptr_t key,const char *path);
void rio_file_name_clear(unsigned kind,uintptr_t key);
void rio_file_name_get(unsigned kind,uintptr_t key,char out[192]);
#endif
