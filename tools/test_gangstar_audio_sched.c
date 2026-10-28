#include "gangstar_audio_sched.h"
#include <assert.h>
#include <stdio.h>
struct State { int current,gets,sets,fail_on; };
static int get(void *p) { struct State *s=p;++s->gets;return s->current; }
static int set(void *p,int priority) { struct State *s=p;++s->sets;if(s->sets==s->fail_on)return -9;s->current=priority;return 0; }
int main(void) {
 struct State state={.current=191};RioAudioScheduleOps ops={get,set,&state};
 RioAudioSchedule s=rio_audio_schedule_begin(&ops);assert(s.changed && state.current==160);
 RioAudioSchedule nested=rio_audio_schedule_begin(&ops);assert(!nested.changed);
 assert(!rio_audio_schedule_end(&nested,&ops) && state.current==160);
 assert(!rio_audio_schedule_end(&s,&ops) && state.current==191 && state.sets==2);
 assert(!rio_audio_schedule_end(&s,&ops) && state.sets==2);
 for(int priority=64;priority<=192;++priority)if(priority!=191) {
  state=(struct State){.current=priority};s=rio_audio_schedule_begin(&ops);
  assert(!s.changed && !state.sets);assert(!rio_audio_schedule_end(&s,&ops));
 }
 state=(struct State){.current=-3};s=rio_audio_schedule_begin(&ops);assert(s.error==-3 && !state.sets);
 state=(struct State){.current=191,.fail_on=1};s=rio_audio_schedule_begin(&ops);
 assert(s.error==-9 && !s.changed && state.current==191);rio_audio_schedule_end(&s,&ops);assert(state.sets==1);
 state=(struct State){.current=191,.fail_on=2};s=rio_audio_schedule_begin(&ops);
 assert(rio_audio_schedule_end(&s,&ops)==-9); /* Caller must report restore failure. */
 puts("Scoped audio priority: exact worker guard, nested restoration, unchanged other priorities, query/change/restore failure reporting passed");
}
