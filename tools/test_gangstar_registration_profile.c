#include "gangstar_registration_profile.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
 RioRegistrationProfile p={0};
 rio_registration_record(&p,1,2,3,4,100);
 rio_registration_record(&p,1,2,3,4,200);
 assert(p.used==1 && p.passes[0].calls==2 && p.passes[0].us==300);
 rio_registration_record(&p,1,5,3,4,10);
 rio_registration_record(&p,1,2,6,4,20);
 rio_registration_record(&p,1,2,3,7,30);
 rio_registration_record(&p,8,2,3,4,40);
 assert(p.used==5 && !p.dropped);
 for(unsigned i=p.used;i<RIO_REGISTER_PASSES;++i)rio_registration_record(&p,100+i,2,3,4,1);
 assert(p.used==RIO_REGISTER_PASSES);
 rio_registration_record(&p,999,2,3,4,50);
 assert(p.dropped==1 && p.used==RIO_REGISTER_PASSES);
 rio_registration_record(&p,1,2,3,4,400);
 assert(p.passes[0].calls==3 && p.passes[0].us==700);
 p=(RioRegistrationProfile){0};
 rio_registration_record(&p,0,0,0,0,0);
 assert(p.used==1 && p.passes[0].calls==1 && !p.dropped);
 puts("Registration profile: distinct root/camera/culler/manager groups, totals, bounded overflow, existing groups retained and reset");
}
