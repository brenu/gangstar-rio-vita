#ifndef GANGSTAR_PHASE_TIMING_H
#define GANGSTAR_PHASE_TIMING_H
#include <stdint.h>
#define RIO_PHASE_DEPTH 16
typedef struct {uint64_t child[RIO_PHASE_DEPTH];unsigned depth,dropped;} RioPhaseStack;
typedef struct {uint64_t inclusive,self,worst;unsigned calls;} RioPhaseTotals;
static inline int rio_phase_enter(RioPhaseStack *s) {
 if(s->depth==RIO_PHASE_DEPTH){++s->dropped;return -1;}
 unsigned slot=s->depth++;s->child[slot]=0;return (int)slot;
}
static inline void rio_phase_leave(RioPhaseStack *s,RioPhaseTotals *t,int slot,uint64_t elapsed) {
 if(slot<0)return;
 uint64_t child=s->child[slot];s->depth=(unsigned)slot;
 if(slot)s->child[slot-1]+=elapsed;
 t->inclusive+=elapsed;t->self+=elapsed>child?elapsed-child:0;++t->calls;
 if(elapsed>t->worst)t->worst=elapsed;
}
#endif
