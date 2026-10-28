#include "gangstar_path_index.h"
#include <assert.h>
#include <stdio.h>
static int compare(const void *a,const void *b) {
    return strcmp(((const RioPakEntry *)a)->name,((const RioPakEntry *)b)->name);
}
static int reference(const RioPakEntry *e,size_t n,const char *q,int fold,
    const char *const *folders,size_t count) {
    int found=rio_pak_find(e,n,q,fold,0);
    if (found!=-1) return found;
    for (size_t i=0;i<count;++i) {
        char path[512]; snprintf(path,sizeof(path),"%s/%s",folders[i],q);
        found=rio_pak_find(e,n,path,fold,0);
        if (found!=-1) return found;
    }
    return -1;
}
int main(void) {
    RioPakEntry entries[]={
        {.name="z/same.tga"},{.name="a/same.tga"},{.name="same.tga"},
        {.name="a/nested/world.tga"},{.name="b/nested/world.tga"},
        {.name="a/only.tga"},{.name="a/only.tga"},{.name="/empty.tga"},
        {.name="a//slash.tga"},{.name="case/mixed.tga"}
    };
    qsort(entries,sizeof(entries)/sizeof(*entries),sizeof(*entries),compare);
    const char *folders[]={"z","a","a","b","","a/","CASE"};
    const char *queries[]={"same.tga","only.tga","nested/world.tga","empty.tga","/slash.tga",
        "absent.tga","a/same.tga","mixed.tga","MiXeD.TGA","world.tga","./only.tga"};
    RioPathIndex index={0}; size_t n=sizeof(entries)/sizeof(*entries);
    for (int fold=0;fold<2;++fold)
    for (unsigned rotation=0;rotation<7;++rotation) {
        assert(rio_path_index_prepare(&index,entries,n,fold,folders,7,rotation+1));
        for (unsigned q=0;q<sizeof(queries)/sizeof(*queries);++q)
            assert(rio_path_index_find(&index,queries[q],0)==reference(entries,n,queries[q],fold,folders,7));
        const char *first=folders[0]; memmove(folders,folders+1,6*sizeof(*folders)); folders[6]=first;
    }
    // Repeated and late search directories, resembling gameplay's >1,200 paths.
    const char **many=malloc(1300*sizeof(*many)); assert(many);
    for (unsigned i=0;i<1300;++i) many[i]="missing";
    many[1299]="a";
    assert(rio_path_index_prepare(&index,entries,n,0,many,1300,100));
    assert(rio_path_index_find(&index,"only.tga",0)==reference(entries,n,"only.tga",0,many,1300));
    unsigned built=index.entry_builds;
    assert(rio_path_index_prepare(&index,entries,n,0,many,1300,100));
    assert(index.entry_builds==built);
    many[1299]="b";
    assert(rio_path_index_prepare(&index,entries,n,0,many,1300,101));
    assert(rio_path_index_find(&index,"only.tga",0)==-1);
    assert(rio_path_index_find(&index,"/unknown.tga",1)==INT_MIN);
    char long_name[257]; memset(long_name,'a',256); long_name[256]=0;
    assert(rio_path_index_find(&index,long_name,0)==INT_MIN);
    assert(!rio_path_index_prepare(&index,entries,65537,0,many,1300,102));
    rio_path_index_clear(&index); free(many);
    // Many bucket collisions, shuffled folder priority, and first duplicates.
    RioPakEntry *large=calloc(5000,sizeof(*large)); assert(large);
    char (*names)[64]=calloc(5000,sizeof(*names)); assert(names);
    const char *dirs[]={"d9","d3","d7","d0","d1","d2","d4","d5","d6","d8"};
    for (unsigned i=0;i<5000;++i) {
        snprintf(names[i],64,"d%u/file%03u.tga",i%10,i/10); large[i].name=names[i];
    }
    qsort(large,5000,sizeof(*large),compare);
    assert(rio_path_index_prepare(&index,large,5000,0,dirs,10,103));
    for (unsigned i=0;i<520;++i) {
        char query[32]; snprintf(query,sizeof(query),"file%03u.tga",i);
        assert(rio_path_index_find(&index,query,0)==reference(large,5000,query,0,dirs,10));
    }
    /* A background index can change its folder priority without changing
       the main index's prepared metadata or chosen entry. */
    RioPathIndex background={0};
    const char *background_dirs[]={"d4","d8"};
    assert(rio_path_index_prepare(&background,large,5000,0,background_dirs,2,200));
    assert(rio_path_index_find(&background,"file123.tga",0)==reference(large,5000,"file123.tga",0,background_dirs,2));
    assert(rio_path_index_find(&index,"file123.tga",0)==reference(large,5000,"file123.tga",0,dirs,10));
    rio_path_index_clear(&background);
    assert(rio_path_index_find(&index,"file123.tga",0)==reference(large,5000,"file123.tga",0,dirs,10));
    rio_path_index_clear(&index); free(names); free(large);
    puts("Direct asset paths: native exact/folder priority, duplicate names, case folding, 1300 folders, collisions, metadata changes and fallback passed");
}
