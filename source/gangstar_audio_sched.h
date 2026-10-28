#ifndef GANGSTAR_AUDIO_SCHED_H
#define GANGSTAR_AUDIO_SCHED_H
/* Scoped experiment: only the observed 191-priority emitter worker qualifies. */
typedef struct { int original,changed,error; } RioAudioSchedule;
typedef struct { int (*get)(void *);int (*set)(void *,int);void *context; } RioAudioScheduleOps;
static inline RioAudioSchedule rio_audio_schedule_begin(const RioAudioScheduleOps *ops) {
    RioAudioSchedule s={ops->get(ops->context),0,0};
    if(s.original<0) { s.error=s.original;return s; }
    if(s.original==191) { s.error=ops->set(ops->context,160);s.changed=s.error>=0; }
    return s;
}
static inline int rio_audio_schedule_end(RioAudioSchedule *s,const RioAudioScheduleOps *ops) {
    if(!s->changed)return 0;
    s->changed=0;return ops->set(ops->context,s->original);
}
#endif
