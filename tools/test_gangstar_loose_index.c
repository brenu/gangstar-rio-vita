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
    snprintf(file,sizeof(file),"%smain.obb",root);
    FILE *obb=fopen(file,"wb");assert(obb);fclose(obb);
    errno=EINTR;assert(rio_loose_index_init(root)==11 && errno==EINTR);
    snprintf(path,sizeof(path),"%s_packEtc/androidWVGA540/_packetc/ground.tga",root);
    assert(rio_loose_index_missing(path));
    snprintf(path,sizeof(path),"%s_packRes/shaders/foo",root);assert(rio_loose_index_missing(path));
    snprintf(path,sizeof(path),"%s_packEtc.backup/foo",root);assert(!rio_loose_index_missing(path));
    snprintf(path,sizeof(path),"%s_packEtc/../saves/foo",root);assert(!rio_loose_index_missing(path));
    snprintf(path,sizeof(path),"%s_packEtc/./foo",root);assert(!rio_loose_index_missing(path));
    snprintf(path,sizeof(path),"%smain.obb",root);assert(!rio_loose_index_missing(path));
    snprintf(path,sizeof(path),"%ssaves/Missions.sav",root);assert(!rio_loose_index_missing(path));
    /* Single names directly in the root: only scanned entries stay live. */
    snprintf(path,sizeof(path),"%srich_00_lightmap00_alpha.tga",root);assert(rio_loose_index_missing(path));
    snprintf(path,sizeof(path),"%s./downtown_00_lightmap00_alpha.tga",root);assert(rio_loose_index_missing(path));
    snprintf(path,sizeof(path),"%s././MAIN.OBB",root);assert(!rio_loose_index_missing(path));
    snprintf(path,sizeof(path),"%s..",root);assert(!rio_loose_index_missing(path));
    snprintf(path,sizeof(path),"%s../x",root);assert(!rio_loose_index_missing(path));
    snprintf(path,sizeof(path),"%s",root);assert(!rio_loose_index_missing(path));
    snprintf(path,sizeof(path),"%ssaves/Missions.sav",root);
    rio_loose_index_mutation(path);assert(rio_loose_index_stats().absent==11);
    assert(!rio_loose_index_missing("_packEtc/foo") && !rio_loose_index_missing(NULL));
    snprintf(asset,sizeof(asset),"%s_packEtc",root);
    rio_loose_index_mutation(asset);assert(!rio_loose_index_stats().absent);
    assert(!mkdir(asset,0700));
    snprintf(file,sizeof(file),"%s/override.tga",asset);
    FILE *f=fopen(file,"wb");assert(f);assert(fwrite("test",1,4,f)==4);assert(!fclose(f));
    assert(rio_loose_index_init(root)==10); /* Existing overrides stay live. */
    assert(!rio_loose_index_missing(file));
    snprintf(path,sizeof(path),"%s_packEtc/nonexistent.tga",root);assert(!rio_loose_index_missing(path));
    snprintf(path,sizeof(path),"%s_packRes/foo",root);assert(rio_loose_index_missing(path));
    rio_loose_index_mutation("relative/path");assert(!rio_loose_index_missing(path));
    assert(rio_loose_index_stats().invalidations==1);
    assert(!unlink(file) && !rmdir(asset));
    /* A case variant or regular file also prevents absence classification. */
    snprintf(asset,sizeof(asset),"%s_packetc",root);f=fopen(asset,"wb");assert(f);fclose(f);
    assert(rio_loose_index_init(root)==10);
    snprintf(path,sizeof(path),"%s_packEtc/foo",root);assert(!rio_loose_index_missing(path));
    assert(!unlink(asset));
    assert(rio_loose_index_init("/no-such-rio-directory/")==0);
    assert(!rio_loose_index_missing(path));
    assert(rio_loose_index_init(NULL)==0);
    /* Relative override directory: proven absent answers probes until a mutation. */
    char cwd[1024];assert(getcwd(cwd,sizeof(cwd)));
    char nested[600];snprintf(nested,sizeof(nested),"%s/app",directory);
    assert(!mkdir(nested,0700) && !chdir(nested));
    assert(rio_loose_index_init(root)==11);
    assert(rio_loose_index_init_overrides("../Documents",NULL)==0); /* needs a trailing separator */
    int probe=0;errno=EINTR;assert(rio_loose_index_init_overrides("../Documents/",&probe)==4 && errno==EINTR && probe==ENOENT);
    assert(rio_loose_index_missing("../Documents/rich_00_lightmap00_alpha.tga"));
    assert(rio_loose_index_missing("../Documents/_packetc/glow_blue_alpha.tga"));
    assert(!rio_loose_index_missing("../Documents/../app/foo"));
    assert(!rio_loose_index_missing("../Documentsx/foo") && !rio_loose_index_missing("Documents/foo"));
    assert(rio_loose_index_stats().absent==15);
    snprintf(path,sizeof(path),"%ssaves/Missions.sav",root);
    rio_loose_index_mutation(path);assert(rio_loose_index_missing("../Documents/foo.tga"));
    rio_loose_index_mutation("../Documents/foo.tga");
    assert(!rio_loose_index_missing("../Documents/foo.tga"));
    /* An existing empty directory is answered too; any entry keeps probes live. */
    char documents[600];snprintf(documents,sizeof(documents),"%s/Documents",directory);
    assert(!mkdir(documents,0700));
    assert(rio_loose_index_init(root)==11 && rio_loose_index_init_overrides("../Documents/",&probe)==4 && !probe);
    assert(rio_loose_index_missing("../Documents/foo.tga"));
    snprintf(file,sizeof(file),"%s/foo.tga",documents);
    f=fopen(file,"wb");assert(f);fclose(f);
    assert(rio_loose_index_init(root)==11 && rio_loose_index_init_overrides("../Documents/",NULL)==0);
    assert(!rio_loose_index_missing("../Documents/foo.tga"));
    assert(!unlink(file) && !rmdir(documents) && !chdir(cwd) && !rmdir(nested));
    /* A root too large to list keeps single-name probes live. */
    snprintf(file,sizeof(file),"%smain.obb",root);assert(!unlink(file));
    for(int i=0;i<70;++i){snprintf(file,sizeof(file),"%sfile%02d",root,i);f=fopen(file,"wb");assert(f);fclose(f);}
    assert(rio_loose_index_init(root)==3);
    snprintf(path,sizeof(path),"%sabsent.tga",root);assert(!rio_loose_index_missing(path));
    for(int i=0;i<70;++i){snprintf(file,sizeof(file),"%sfile%02d",root,i);assert(!unlink(file));}
    assert(!rmdir(directory));
    puts("Loose directory index: proven absent roots, existing overrides/case variants, saves, mutation invalidation, dot-component guards, failed-scan fallback and absent or empty relative override directory and root name listing passed");
}
