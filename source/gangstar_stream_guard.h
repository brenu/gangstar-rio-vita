#ifndef GANGSTAR_STREAM_GUARD_H
#define GANGSTAR_STREAM_GUARD_H
typedef struct {
 void *(*backend)(void *);
 void (*reopen)(void *);
 int (*query)(void *);
 void (*report)(void *,int);
} RioStreamQuery;
static inline int rio_stream_query(void *file,const RioStreamQuery *ops) {
 void *stream=ops->backend(file);
 if(!stream) {
  ops->reopen(file);
  stream=ops->backend(file);
  if(ops->report)ops->report(file,stream!=0);
 }
 return stream?ops->query(stream):-1;
}
#endif
