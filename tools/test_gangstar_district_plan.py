from pathlib import Path
import subprocess
src=Path('source/gangstar_district_preload.c').read_text()
a=src.index(' unsigned plan[384]');b=src.index(' // Priority changes',a)
s='''#include "gangstar_district_policy.h"
#include "gangstar_texture_lod.h"
#include "gangstar_district_manifest.h"
#include <assert.h>
#define ASSETS (sizeof(rio_district_assets)/sizeof(*rio_district_assets))
static void test(unsigned current,unsigned next) {
'''+src[a:b]+'''
 assert(total>0 && total<=384 && bytes<=60u*1024u*1024u && shared<=12u*1024u*1024u);
 for(unsigned j=0;j<total;++j) {
  unsigned mask=rio_district_name(rio_district_assets[plan[j]].name,0);
  assert(!mask || (mask&(current|next)) || rio_district_shared_map(rio_district_assets[plan[j]].name));
  assert(priorities[j]>=2 && priorities[j]<=4);
  const char *name=rio_district_assets[plan[j]].name;
  if(rio_lod_original_source(name))assert(mask==RIO_D_COPA || !strcmp(name+strlen(name)-6,"00.tga"));
  for(unsigned k=0;k<j;++k)assert(plan[j]!=plan[k]);
 }
 if(current==RIO_D_COPA) {
  unsigned lightmaps=0;
  for(unsigned j=0;j<total;++j)if(rio_district_name(rio_district_assets[plan[j]].name,0)==RIO_D_COPA && rio_lod_original_source(rio_district_assets[plan[j]].name))++lightmaps;
  assert(lightmaps==4);
 }
}
int main(void) {
 for(unsigned i=1;i<=RIO_D_INTERIOR;i<<=1)
  for(unsigned j=1;j<=RIO_D_INTERIOR;j<<=1)test(i,j==i?0:j);
 test(RIO_D_COPA|RIO_D_MIDDLE,RIO_D_CITY);
}
'''
p=Path('work/tests/district_plan.c');p.write_text(s)
subprocess.run(['cc','-std=c11','-D_GNU_SOURCE','-DGANGSTAR_WORLD_TEXTURE_LOD','-DGANGSTAR_TEXTURE_LOD_EDGE=512','-O2','-Wall','-Wextra','-Werror','-I','source',str(p),'-lm','-o','work/tests/district_plan'],check=True)
subprocess.run(['work/tests/district_plan'],check=True)
print('District plans: all current/upcoming pairs, exact grouping, no duplicate jobs, current lightmaps retained and 60MiB/12MiB bounds passed')
