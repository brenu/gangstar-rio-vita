#include "gangstar_loose_index.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
int main(void) {
    char directory[]="/tmp/rio-loose-index-XXXXXX";
    assert(mkdtemp(directory));
    char root[512],path[1024],asset[1024],file[1100];
    snprintf(root,sizeof(root),"%s/",directory);
    errno=EINTR;assert(rio_loose_index_init(root)==3 && errno==EINTR);
    snprintf(path,sizeof(path),"%s_packEtc/androidWVGA540/_packetc/ground.tga",root);
    assert(rio_loose_index_missing(path));
    snprintf(path,sizeof(path),"%s_packRes/shaders/foo",root);assert(rio_loose_index_missing(path));
    snprintf(path,sizeof(path),"%s_packEtc.backup/foo",root);assert(!rio_loose_index_missing(path));
    snprintf(path,sizeof(path),"%s_packEtc/../saves/foo",root);assert(!rio_loose_index_missing(path));
    snprintf(path,sizeof(path),"%s_packEtc/./foo",root);assert(!rio_loose_index_missing(path));
    snprintf(path,sizeof(path),"%smain.obb",root);assert(!rio_loose_index_missing(path));
    snprintf(path,sizeof(path),"%ssaves/Missions.sav",root);assert(!rio_loose_index_missing(path));
    rio_loose_index_mutation(path);assert(rio_loose_index_stats().absent==3);
    assert(!rio_loose_index_missing("_packEtc/foo") && !rio_loose_index_missing(NULL));
    snprintf(asset,sizeof(asset),"%s_packEtc",root);
    rio_loose_index_mutation(asset);assert(!rio_loose_index_stats().absent);
    assert(!mkdir(asset,0700));
    snprintf(file,sizeof(file),"%s/override.tga",asset);
    FILE *f=fopen(file,"wb");assert(f);assert(fwrite("test",1,4,f)==4);assert(!fclose(f));
    assert(rio_loose_index_init(root)==2); /* Existing overrides stay live. */
    assert(!rio_loose_index_missing(file));
    snprintf(path,sizeof(path),"%s_packEtc/nonexistent.tga",root);assert(!rio_loose_index_missing(path));
    snprintf(path,sizeof(path),"%s_packRes/foo",root);assert(rio_loose_index_missing(path));
    rio_loose_index_mutation("relative/path");assert(!rio_loose_index_missing(path));
    assert(rio_loose_index_stats().invalidations==1);
    assert(!unlink(file) && !rmdir(asset));
    /* A case variant or regular file also prevents absence classification. */
    snprintf(asset,sizeof(asset),"%s_packetc",root);f=fopen(asset,"wb");assert(f);fclose(f);
    assert(rio_loose_index_init(root)==2);
    snprintf(path,sizeof(path),"%s_packEtc/foo",root);assert(!rio_loose_index_missing(path));
    assert(!unlink(asset));
    assert(rio_loose_index_init("/no-such-rio-directory/")==0);
    assert(!rio_loose_index_missing(path));
    assert(rio_loose_index_init(NULL)==0);
    assert(!rmdir(directory));
    puts("Loose directory index: proven absent roots, existing overrides/case variants, saves, mutation invalidation, dot-component guards and failed-scan fallback passed");
}
