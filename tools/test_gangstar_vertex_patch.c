#include "vertex_patch_cache.h"
#include <assert.h>
#include <stdio.h>
static RioVertexPatchCache cache;
static unsigned char a[32],s[16];
static int match(unsigned p,uintptr_t patcher,uintptr_t shader,uintptr_t source,uintptr_t output){return rio_vertex_patch_match(&cache,p,patcher,shader,source,a,sizeof(a),s,sizeof(s),output);}
int main(void){
 assert(!match(7,1,2,3,4));
 rio_vertex_patch_store(&cache,7,1,2,3,a,sizeof(a),s,sizeof(s),4);
 assert(match(7,1,2,3,4));
 for(unsigned i=0;i<sizeof(a);++i){a[i]=1;assert(!match(7,1,2,3,4));a[i]=0;}
 for(unsigned i=0;i<sizeof(s);++i){s[i]=1;assert(!match(7,1,2,3,4));s[i]=0;}
 assert(!match(8,1,2,3,4) && !match(7,9,2,3,4) && !match(7,1,9,3,4) && !match(7,1,2,9,4) && !match(7,1,2,3,9));
 assert(!rio_vertex_patch_match(&cache,7,1,2,3,a,31,s,16,4));
 rio_vertex_patch_invalidate(&cache,7);assert(!match(7,1,2,3,4));
 rio_vertex_patch_store(&cache,7,1,2,3,a,sizeof(a),s,sizeof(s),4);
 unsigned collision=7+RIO_VERTEX_PATCH_SLOTS;
 rio_vertex_patch_store(&cache,collision,1,2,3,a,sizeof(a),s,sizeof(s),4);
 assert(!match(7,1,2,3,4) && match(collision,1,2,3,4));
 rio_vertex_patch_invalidate(&cache,7);assert(match(collision,1,2,3,4));
 rio_vertex_patch_store(&cache,7,1,2,3,a,RIO_VERTEX_PATCH_BYTES+1,s,sizeof(s),4);
 assert(!match(7,1,2,3,4));
 for(unsigned i=0;i<10000;++i){rio_vertex_patch_store(&cache,i+1,1,2,3,a,sizeof(a),s,sizeof(s),4);assert(match(i+1,1,2,3,4));}
 memset(&cache,0,sizeof(cache));assert(!match(7,1,2,3,4));
 puts("Vertex patch cache: every layout byte, shader/source/patcher/output identity, relink/delete invalidation, collisions, size bounds and 10000 program lifetimes passed");
}
