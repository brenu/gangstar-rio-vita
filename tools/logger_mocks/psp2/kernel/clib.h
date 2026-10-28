#include <stdarg.h>
#include <stddef.h>
int sceClibPrintf(const char *,...);
int sceClibSnprintf(char *,size_t,const char *,...);
int sceClibVsnprintf(char *,size_t,const char *,va_list);
