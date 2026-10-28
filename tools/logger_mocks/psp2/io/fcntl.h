#include <stddef.h>
typedef int SceUID;
enum {SCE_O_WRONLY=1,SCE_O_CREAT=2,SCE_O_TRUNC=4};
int sceIoOpen(const char *,int,int);
int sceIoWrite(int,const void *,size_t);
int sceIoSyncByFd(int,int);
int sceIoClose(int);
