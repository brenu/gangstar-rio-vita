/* Gangstar string references: immutable storage, real local/global ownership
 * and borrowed character views. No per-query character copies are allocated. */
#include "gangstar_jni_strings.h"
#include "reimpl/bits/object_set.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
typedef struct LocalRef {uintptr_t owner;unsigned depth,count;struct LocalRef *next;} LocalRef;
typedef struct {
 LocalRef local;unsigned locals,globals,pins,constant;size_t units,utf_bytes,allocation;
 jchar *wide;char *utf;unsigned char data[];
} String;
static GangstarObjectSet strings;
static String *constants[64];
static unsigned constant_count;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static struct JNINativeInterface previous;
static uintptr_t (*current_thread)(void);
static struct {uintptr_t owner;unsigned depth;} scopes[128];
static RioJniStringStats stats;
static unsigned *depth_for(uintptr_t owner) {
 for(unsigned i=0;i<128;++i)if(scopes[i].owner==owner || !scopes[i].owner) {
  scopes[i].owner=owner;return &scopes[i].depth;
 }
 return NULL;
}
static String *lookup(void *p){return gangstar_object_contains(&strings,p)?p:NULL;}
static void destroy_if_unused(String *s) {
 if(s->locals || s->globals || s->pins)return;
 gangstar_object_remove(&strings,s);
 LocalRef *r=s->local.next;
 while(r){LocalRef *next=r->next;free(r);r=next;}
 --stats.live;stats.bytes-=s->allocation;++stats.destroyed;free(s);
}
static int add_local(String *s,uintptr_t owner,unsigned depth) {
 LocalRef *r=&s->local,*empty=NULL;
 for(;r;r=r->next) {
  if(!r->count && !empty)empty=r;
  if(r->count && r->owner==owner && r->depth==depth)break;
 }
 if(!r) {
  if(empty)r=empty;
  else {r=calloc(1,sizeof(*r));if(!r)return 0;r->next=s->local.next;s->local.next=r;}
  r->owner=owner;r->depth=depth;
 }
 ++r->count;++s->locals;return 1;
}
static void delete_local(String *s,uintptr_t owner,unsigned depth) {
 LocalRef *best=NULL;
 for(LocalRef *r=&s->local;r;r=r->next)
  if(r->count && r->owner==owner && r->depth<=depth && (!best || r->depth>best->depth))best=r;
 if(best){--best->count;--s->locals;}
 destroy_if_unused(s);
}
static void sweep(uintptr_t owner,unsigned depth) {
 String *dead[GANGSTAR_OBJECT_SET_SIZE];size_t count=0;
 for(unsigned i=0;i<GANGSTAR_OBJECT_SET_SIZE;++i)if(strings.slots[i]) {
  String *s=(String *)strings.slots[i];
  for(LocalRef *r=&s->local;r;r=r->next)if(r->count && r->owner==owner && r->depth>=depth) {
   s->locals-=r->count;r->count=0;
  }
  if(!s->locals && !s->globals && !s->pins)dead[count++]=s;
 }
 for(size_t i=0;i<count;++i)destroy_if_unused(dead[i]);
}
// JNI uses modified UTF-8: embedded NUL and UTF-16 surrogates are encoded as
// individual code units. Also accept four-byte UTF-8 supplied by game assets.
static size_t decode(const unsigned char *p,jchar *out) {
 size_t n=0;
 while(*p) {
  unsigned c=*p++,v;
  if(c<128)v=c;
  else if((c&0xe0)==0xc0 && (p[0]&0xc0)==0x80){v=((c&31)<<6)|(p[0]&63);p++;}
  else if((c&0xf0)==0xe0 && p[0] && p[1] && (p[0]&0xc0)==0x80 && (p[1]&0xc0)==0x80) {
   v=((c&15)<<12)|((p[0]&63)<<6)|(p[1]&63);p+=2;
  } else if((c&0xf8)==0xf0 && p[0] && p[1] && p[2] && (p[0]&0xc0)==0x80 &&
      (p[1]&0xc0)==0x80 && (p[2]&0xc0)==0x80) {
   v=((c&7)<<18)|((p[0]&63)<<12)|((p[1]&63)<<6)|(p[2]&63);p+=3;
   if(v>=0x10000 && v<=0x10ffff){v-=0x10000;if(out)out[n]=(jchar)(0xd800+(v>>10));++n;v=0xdc00+(v&1023);}
   else v=0xfffd;
  } else v=0xfffd;
  if(out)out[n]=(jchar)v;
  ++n;
 }
 return n;
}
static size_t encode(const jchar *p,size_t n,char *out) {
 size_t bytes=0;
 for(size_t i=0;i<n;++i){unsigned v=p[i];
  if(v && v<128){if(out)out[bytes]=(char)v;++bytes;}
  else if(v<2048){if(out){out[bytes]=(char)(0xc0|(v>>6));out[bytes+1]=(char)(0x80|(v&63));}bytes+=2;}
  else {if(out){out[bytes]=(char)(0xe0|(v>>12));out[bytes+1]=(char)(0x80|((v>>6)&63));out[bytes+2]=(char)(0x80|(v&63));}bytes+=3;}
 }
 return bytes;
}
static String *allocate(size_t units) {
 if(units>INT_MAX || units>(SIZE_MAX-sizeof(String)-1)/5)return NULL;
 size_t bytes=sizeof(String)+5*units+1;String *s=calloc(1,bytes);if(!s)return NULL;
 s->units=units;s->allocation=bytes;s->wide=(jchar *)s->data;s->utf=(char *)(s->data+2*units);return s;
}
static jstring publish(String *s) {
 if(!s)return NULL;
 s->utf_bytes=encode(s->wide,s->units,s->utf);s->utf[s->utf_bytes]=0;
 pthread_mutex_lock(&lock);uintptr_t owner=current_thread();unsigned *depth=depth_for(owner);
 int ok=depth && gangstar_object_add(&strings,s);
 if(ok){add_local(s,owner,*depth);++stats.live;stats.bytes+=s->allocation;++stats.created;}
 pthread_mutex_unlock(&lock);if(!ok){free(s);return NULL;}return s;
}
static jstring new_utf(JNIEnv *e,const char *p){(void)e;if(!p)return NULL;String *s=allocate(decode((const unsigned char *)p,NULL));if(s)decode((const unsigned char *)p,s->wide);return publish(s);}
static jstring new_wide(JNIEnv *e,const jchar *p,jsize n){(void)e;if(n<0 || (!p && n))return NULL;String *s=allocate((size_t)n);if(s && n)memcpy(s->wide,p,(size_t)n*2);return publish(s);}
static jobject global_ref(JNIEnv *e,jobject p) {
 pthread_mutex_lock(&lock);String *s=lookup(p);if(s){++s->globals;++stats.globals;}
 pthread_mutex_unlock(&lock);return s?p:previous.NewGlobalRef(e,p);
}
static jobject local_ref(JNIEnv *e,jobject p) {
 pthread_mutex_lock(&lock);String *s=lookup(p);int ok=1;
 if(s){uintptr_t owner=current_thread();unsigned *depth=depth_for(owner);ok=depth && add_local(s,owner,*depth);}
 pthread_mutex_unlock(&lock);return s?(ok?p:NULL):previous.NewLocalRef(e,p);
}
static void release_global(JNIEnv *e,jobject p) {
 pthread_mutex_lock(&lock);String *s=lookup(p);
 if(s){if(s->globals>(s->constant?1u:0u)){--s->globals;--stats.globals;}destroy_if_unused(s);}
 pthread_mutex_unlock(&lock);if(!s)previous.DeleteGlobalRef(e,p);
}
static void release_local(JNIEnv *e,jobject p) {
 pthread_mutex_lock(&lock);String *s=lookup(p);
 if(s){uintptr_t owner=current_thread();unsigned *depth=depth_for(owner);if(depth)delete_local(s,owner,*depth);}
 pthread_mutex_unlock(&lock);if(!s)previous.DeleteLocalRef(e,p);
}
static const char *utf_chars(JNIEnv *e,jstring p,jboolean *copy) {
 pthread_mutex_lock(&lock);String *s=lookup(p);const char *value=NULL;
 if(s){if(!s->constant){++s->pins;++stats.pins;}value=s->utf;if(copy)*copy=JNI_FALSE;}
 pthread_mutex_unlock(&lock);return s?value:previous.GetStringUTFChars(e,p,copy);
}
static const jchar *wide_chars(JNIEnv *e,jstring p,jboolean *copy) {
 pthread_mutex_lock(&lock);String *s=lookup(p);const jchar *value=NULL;
 if(s){if(!s->constant){++s->pins;++stats.pins;}value=s->wide;if(copy)*copy=JNI_FALSE;}
 pthread_mutex_unlock(&lock);return s?value:previous.GetStringChars(e,p,copy);
}
static void release_utf(JNIEnv *e,jstring p,char *value) {
 pthread_mutex_lock(&lock);String *s=lookup(p);
 if(s && !s->constant && value==s->utf && s->pins){--s->pins;--stats.pins;destroy_if_unused(s);}
 pthread_mutex_unlock(&lock);if(!s)previous.ReleaseStringUTFChars(e,p,value);
}
static void release_wide(JNIEnv *e,jstring p,const jchar *value) {
 pthread_mutex_lock(&lock);String *s=lookup(p);
 if(s && !s->constant && value==s->wide && s->pins){--s->pins;--stats.pins;destroy_if_unused(s);}
 pthread_mutex_unlock(&lock);if(!s)previous.ReleaseStringChars(e,p,value);
}
static jsize length(JNIEnv *e,jstring p){pthread_mutex_lock(&lock);String *s=lookup(p);jsize n=s?(jsize)s->units:0;pthread_mutex_unlock(&lock);return s?n:previous.GetStringLength(e,p);}
static jsize utf_length(JNIEnv *e,jstring p){pthread_mutex_lock(&lock);String *s=lookup(p);jsize n=s?(jsize)s->utf_bytes:0;pthread_mutex_unlock(&lock);return s?n:previous.GetStringUTFLength(e,p);}
static void wide_region(JNIEnv *e,jstring p,jsize start,jsize n,jchar *out) {
 pthread_mutex_lock(&lock);String *s=lookup(p);
 if(s && out && start>=0 && n>=0 && (size_t)start<=s->units && (size_t)n<=s->units-(size_t)start)memcpy(out,s->wide+start,(size_t)n*2);
 pthread_mutex_unlock(&lock);if(!s)previous.GetStringRegion(e,p,start,n,out);
}
static void utf_region(JNIEnv *e,jstring p,jsize start,jsize n,char *out) {
 pthread_mutex_lock(&lock);String *s=lookup(p);
 if(s && out && start>=0 && n>=0 && (size_t)start<=s->units && (size_t)n<=s->units-(size_t)start)encode(s->wide+start,(size_t)n,out);
 pthread_mutex_unlock(&lock);if(!s)previous.GetStringUTFRegion(e,p,start,n,out);
}
static jint push_frame(JNIEnv *e,jint capacity){(void)e;if(capacity<0)return JNI_ERR;pthread_mutex_lock(&lock);unsigned *depth=depth_for(current_thread());if(depth)++*depth;pthread_mutex_unlock(&lock);return depth?JNI_OK:JNI_ERR;}
static jobject pop_frame(JNIEnv *e,jobject result) {
 (void)e;pthread_mutex_lock(&lock);uintptr_t owner=current_thread();unsigned *depth=depth_for(owner);String *s=lookup(result);
 if(s)++s->pins;
 if(depth){sweep(owner,*depth);if(*depth)--*depth;}
 int ok=!s || (depth && add_local(s,owner,*depth));
 if(s){--s->pins;destroy_if_unused(s);}
 pthread_mutex_unlock(&lock);return ok?result:NULL;
}
// Platform callbacks return fixed values, like Android's Build.MODEL. The
// game intentionally keeps their character views without ReleaseStringChars.
// Root one immutable value per distinct response instead of allocating per draw.
jstring gangstar_jni_string_constant(const char *value) {
 if(!value)return NULL;
 pthread_mutex_lock(&lock);
 uintptr_t owner=current_thread();unsigned *depth=depth_for(owner);
 for(unsigned i=0;i<constant_count;++i)if(!strcmp(constants[i]->utf,value)) {
  String *s=constants[i];int ok=depth && add_local(s,owner,*depth);
  pthread_mutex_unlock(&lock);return ok?s:NULL;
 }
 if(constant_count==64){pthread_mutex_unlock(&lock);return NULL;}
 pthread_mutex_unlock(&lock);
 String *candidate=new_utf(NULL,value);if(!candidate)return NULL;
 pthread_mutex_lock(&lock);owner=current_thread();depth=depth_for(owner);
 // Another thread may have published the same response while we allocated.
 for(unsigned i=0;i<constant_count;++i)if(!strcmp(constants[i]->utf,value)) {
  String *s=constants[i];int ok=depth && add_local(s,owner,*depth);
  if(depth)delete_local(candidate,owner,*depth);
  pthread_mutex_unlock(&lock);return ok?s:NULL;
 }
 if(constant_count==64) {
  if(depth)delete_local(candidate,owner,*depth);
  pthread_mutex_unlock(&lock);return NULL;
 }
 candidate->constant=1;candidate->globals=1;++stats.globals;
 constants[constant_count++]=candidate;
 pthread_mutex_unlock(&lock);return candidate;
}
void gangstar_jni_strings_frame_end(void) {
 pthread_mutex_lock(&lock);uintptr_t owner=current_thread();sweep(owner,0);unsigned *depth=depth_for(owner);if(depth)*depth=0;pthread_mutex_unlock(&lock);
}
RioJniStringStats gangstar_jni_strings_stats(void){pthread_mutex_lock(&lock);RioJniStringStats s=stats;pthread_mutex_unlock(&lock);return s;}
void gangstar_jni_strings_install(struct JNINativeInterface *t,uintptr_t (*thread_id)(void)) {
 previous=*t;current_thread=thread_id;
 t->NewStringUTF=new_utf;t->NewString=new_wide;t->GetStringLength=length;t->GetStringUTFLength=utf_length;
 t->NewGlobalRef=global_ref;t->NewLocalRef=local_ref;t->DeleteGlobalRef=release_global;t->DeleteLocalRef=release_local;
 t->GetStringUTFChars=utf_chars;t->ReleaseStringUTFChars=release_utf;t->GetStringChars=wide_chars;t->ReleaseStringChars=release_wide;
 t->GetStringCritical=wide_chars;t->ReleaseStringCritical=release_wide;t->GetStringRegion=wide_region;t->GetStringUTFRegion=utf_region;
 t->PushLocalFrame=push_frame;t->PopLocalFrame=pop_frame;
}
