#ifndef GANGSTAR_DISTRICT_POLICY_H
#define GANGSTAR_DISTRICT_POLICY_H
#include <stdint.h>
#include <string.h>
#include <math.h>
enum { RIO_D_COPA=1u,RIO_D_MIDDLE=2u,RIO_D_CITY=4u,RIO_D_RICH=8u,
 RIO_D_POOR=16u,RIO_D_AIRPORT=32u,RIO_D_INDUSTRIAL=64u,RIO_D_CLIFF=128u,
 RIO_D_ISLAND=256u,RIO_D_INTERIOR=512u };
static inline unsigned rio_district_name(const char *name,int zone) {
 char key[192];unsigned n=0;
 if(!name)return 0;
 for(unsigned i=0;name[i] && i<191;++i) {
  unsigned char c=(unsigned char)name[i];if(c>='A' && c<='Z')c+=32;
  if(c!='_' && c!='/' && c!='\\')key[n++]=(char)c;
 }key[n]=0;
 if(strstr(key,"interior") || strstr(key,"safehouse"))return RIO_D_INTERIOR;
 if(strstr(key,"copacabana") || strstr(key,"groundcopa"))return RIO_D_COPA;
 if(strstr(key,"middle"))return RIO_D_MIDDLE;
 if(strstr(key,"downtown") || (zone && strstr(key,"envzonecity")))return RIO_D_CITY;
 if(strstr(key,"rich"))return RIO_D_RICH;
 if(strstr(key,"favela") || strstr(key,"cidadededeus") || strstr(key,"cityofgod") || (zone && strstr(key,"poor")))return RIO_D_POOR;
 if(strstr(key,"airport") || strstr(key,"runway"))return RIO_D_AIRPORT;
 if(strstr(key,"industrial"))return RIO_D_INDUSTRIAL;
 if(strstr(key,"mountain") || strstr(key,"sugarloaf") || strstr(key,"corcovado") || (zone && strstr(key,"cliff")))return RIO_D_CLIFF;
 if(strstr(key,"ilhadascobras"))return RIO_D_ISLAND;
 return 0;
}
// These primary maps are visible across zone boundaries (observed in runs).
static inline int rio_district_shared_map(const char *name) {
 return !strcmp(name,"mountain_lightmap00.tga") || !strcmp(name,"sugarloaf_lightmap00.tga") ||
  !strcmp(name,"ground_lowres_lightmap00.tga");
}
static inline int rio_district_contains(const float bounds[6],const float p[3]) {
 for(unsigned i=0;i<3;++i)if(!isfinite(p[i]) || !isfinite(bounds[i]) || !isfinite(bounds[i+3]) ||
  bounds[i]>bounds[i+3] || p[i]<bounds[i] || p[i]>bounds[i+3])return 0;
 return 1;
}
static inline void rio_district_predict(const float now[3],const float previous[3],float seconds,float out[3]) {
 for(unsigned i=0;i<3;++i) {
  float delta=seconds>0 && seconds<2 ? (now[i]-previous[i])*8/seconds : 0;
  if(!isfinite(delta))delta=0;
  out[i]=now[i]+fmaxf(-40000,fminf(40000,delta));
 }
}
#endif
