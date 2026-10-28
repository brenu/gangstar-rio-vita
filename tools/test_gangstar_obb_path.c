#include "gangstar_obb_path.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
 const char *main="ux0:/data/gangstarrio/main.12030.com.gameloft.android.ANMP.GloftG4HM.obb";
 const char *patch="ux0:/data/gangstarrio/patch.12136.com.gameloft.android.ANMP.GloftG4HM.obb";
 char path[512];
 assert(!rio_obb_child_path(main) && !rio_obb_child_path(patch));
 snprintf(path,sizeof(path),"%s/_packres/anim_ingame_male.bdae",main);assert(rio_obb_child_path(path));
 snprintf(path,sizeof(path),"%s/",patch);assert(rio_obb_child_path(path));
 snprintf(path,sizeof(path),"%s/_packetc/test.tga",patch);assert(rio_obb_child_path(path));
 snprintf(path,sizeof(path),"%s.backup/asset",main);assert(!rio_obb_child_path(path));
 assert(!rio_obb_child_path("ux0:/data/gangstarrio/other.obb/asset"));
 assert(!rio_obb_child_path("ux0:/data/gangstarrio/saves/Missions.sav"));
 assert(!rio_obb_child_path("ux0:/data/gangstarrio/_packEtc/asset.tga"));
 assert(!rio_obb_child_path("../Documents/other.obb/asset"));
 assert(!rio_obb_child_path("main.12030.com.gameloft.android.ANMP.GloftG4HM.obb/asset"));
 assert(!rio_obb_child_path("ux0:/data/gangstarrio/main.12031.com.gameloft.android.ANMP.GloftG4HM.obb/asset"));
 assert(!rio_obb_child_path(NULL) && !rio_obb_child_path("") && !rio_obb_child_path("ux0:"));
 assert(rio_obb_path_error(main)==0);
 snprintf(path,sizeof(path),"%s/_packres/test.bdae",main);assert(rio_obb_path_error(path)==ENOTDIR);
 snprintf(path,sizeof(path),"ux0:/data/gangstarrio/_packEtc/%s/_packres/anim_ingame_male.bdae",main);
 assert(rio_obb_path_error(path)==EINVAL);
 snprintf(path,sizeof(path),"ux0:/data/gangstarrio/_packEtc/androidWVGA540/%s/_packres/anim_ingame_female.bdae",main);
 assert(rio_obb_path_error(path)==EINVAL);
 snprintf(path,sizeof(path),"ux0:/data/gangstarrio/_packRes/lowmem/%s/asset",patch);
 assert(rio_obb_path_error(path)==EINVAL);
 snprintf(path,sizeof(path),"ux0:/data/gangstarrio/_packEtc/%s",main);
 assert(!rio_obb_path_error(path)); /* No child suffix: outside this measured policy. */
 snprintf(path,sizeof(path),"ux0:/data/gangstarrio/_packEtc/%s.backup/asset",main);
 assert(!rio_obb_path_error(path));
 snprintf(path,sizeof(path),"ux0:/data/gangstarrio/_packEtc/prefix%s/asset",main);
 assert(!rio_obb_path_error(path));
 snprintf(path,sizeof(path),"ux0:/data/other/_packEtc/%s/asset",main);
 assert(!rio_obb_path_error(path));
 assert(!rio_obb_path_error("ux0:/data/gangstarrio/_packEtc/ux0:/data/gangstarrio/other.obb/asset"));
 puts("OBB child paths: exact known archive children rejected; archive roots, loose overrides, saves, other archives and lookalikes preserved");
}
