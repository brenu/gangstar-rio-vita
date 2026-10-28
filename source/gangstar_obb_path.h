#ifndef GANGSTAR_OBB_PATH_H
#define GANGSTAR_OBB_PATH_H
#include <string.h>
#include <errno.h>
/* Match only known original archive children. Archive roots remain openable.
   An absolute archive child appended to a loose candidate is also invalid;
   preserve the distinct errors observed for direct and embedded paths. */
static inline int rio_obb_path_error(const char *path) {
    static const char *const archives[]={
        "ux0:/data/gangstarrio/main.12030.com.gameloft.android.ANMP.GloftG4HM.obb",
        "ux0:/data/gangstarrio/patch.12136.com.gameloft.android.ANMP.GloftG4HM.obb"
    };
    static const char root[]="ux0:/data/gangstarrio/";
    if(!path)return 0;
    for(unsigned i=0;i<sizeof(archives)/sizeof(*archives);++i) {
        size_t n=strlen(archives[i]);
        if(!strncmp(path,archives[i],n) && path[n]=='/')return ENOTDIR;
        if(strncmp(path,root,sizeof(root)-1))continue;
        const char *at=path;
        while((at=strstr(at,archives[i]))!=NULL) {
            if(at>path+sizeof(root)-1 && at[-1]=='/' && at[n]=='/')return EINVAL;
            ++at;
        }
    }
    return 0;
}
static inline int rio_obb_child_path(const char *path) {
    return rio_obb_path_error(path)!=0;
}
#endif
