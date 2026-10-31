#ifndef RIO_REGISTRATION_PROFILE_H
#define RIO_REGISTRATION_PROFILE_H
#include <stdint.h>
#define RIO_REGISTER_PASSES 16u
typedef struct {uintptr_t manager,root,camera,culler;uint64_t us;unsigned calls;} RioRegistrationPass;
typedef struct {RioRegistrationPass passes[RIO_REGISTER_PASSES];unsigned used,dropped;} RioRegistrationProfile;
static inline void rio_registration_record(RioRegistrationProfile *profile,
 uintptr_t manager,uintptr_t root,uintptr_t camera,uintptr_t culler,uint64_t us) {
 unsigned i;
 for(i=0;i<profile->used;++i) {
  RioRegistrationPass *p=&profile->passes[i];
  if(p->manager==manager && p->root==root && p->camera==camera && p->culler==culler)break;
 }
 if(i==profile->used) {
  if(i==RIO_REGISTER_PASSES){++profile->dropped;return;}
  profile->passes[i]=(RioRegistrationPass){.manager=manager,.root=root,.camera=camera,.culler=culler};
  ++profile->used;
 }
 ++profile->passes[i].calls;profile->passes[i].us+=us;
}
#endif
