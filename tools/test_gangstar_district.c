#include "gangstar_district_policy.h"
#include "gangstar_texture_lod.h"
#include "gangstar_district_manifest.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
 assert(rio_district_name("EnvZoneCopacabana_00",1)==RIO_D_COPA);
 assert(rio_district_name("EnvZoneCity_03",1)==RIO_D_CITY);
 assert(rio_district_name("EnvZoneMiddle_01",1)==RIO_D_MIDDLE);
 assert(rio_district_name("_packetc/downtown_01_lightmap00.tga",0)==RIO_D_CITY);
 assert(rio_district_name("southfavela_lightmap00.tga",0)==RIO_D_POOR);
 assert(!rio_district_name("EnvZoneDefault",1));
 assert(!rio_district_name("ground_00.tga",0));
 float bounds[]={0,0,0,100,100,100},p[]={90,50,50},prev[]={80,50,50},future[3];
 assert(rio_district_contains(bounds,p));
 rio_district_predict(p,prev,0.5f,future);assert(future[0]==250 && !rio_district_contains(bounds,future));
 rio_district_predict(p,prev,0,future);assert(future[0]==90);
 prev[0]=-10000;rio_district_predict(p,prev,0.1f,future);assert(future[0]==40090);
 float fast[]={3907.5f,18802.1f,1033.6f},older[]={2803.4f,18375.9f,1026.2f};
 rio_district_predict(fast,older,0.5f,future);assert(future[0]>20000); // measured route must predict beyond old 250-unit clamp
 p[1]=NAN;assert(!rio_district_contains(bounds,p));
 unsigned count=sizeof(rio_district_assets)/sizeof(*rio_district_assets),copa=0;
 assert(count==322);
 for(unsigned i=0;i<count;++i) {
  assert(rio_lod_name(rio_district_assets[i].name));
  assert(rio_district_assets[i].offset<2119380688u && rio_district_assets[i].size<=2119380688u-rio_district_assets[i].offset);
  if(rio_district_name(rio_district_assets[i].name,0)==RIO_D_COPA)++copa;
 }
 assert(copa>4);
 puts("District policy: native zone aliases, asset groups, bounds, movement prediction, teleport clamp and all 322 catalog locations passed");
}
