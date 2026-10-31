#include "gangstar_phase_timing.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
 RioPhaseStack s={0};RioPhaseTotals outer={0},inner={0},leaf={0};
 int a=rio_phase_enter(&s),b=rio_phase_enter(&s),c=rio_phase_enter(&s);
 rio_phase_leave(&s,&leaf,c,20);rio_phase_leave(&s,&inner,b,60);rio_phase_leave(&s,&outer,a,100);
 assert(!s.depth && outer.inclusive==100 && outer.self==40);
 assert(inner.inclusive==60 && inner.self==40 && leaf.self==20);
 a=rio_phase_enter(&s);rio_phase_leave(&s,&outer,a,50);
 assert(outer.calls==2 && outer.inclusive==150 && outer.self==90 && outer.worst==100);
 for(unsigned i=0;i<RIO_PHASE_DEPTH;++i)assert(rio_phase_enter(&s)==(int)i);
 assert(rio_phase_enter(&s)==-1 && s.dropped==1);
 rio_phase_leave(&s,&leaf,-1,100);assert(s.depth==RIO_PHASE_DEPTH);
 for(int i=RIO_PHASE_DEPTH-1;i>=0;--i)rio_phase_leave(&s,&leaf,i,10);
 assert(!s.depth);
 puts("Phase timing: inclusive/self accounting, nested children, frame reuse, maxima and bounded recursion passed");
}
