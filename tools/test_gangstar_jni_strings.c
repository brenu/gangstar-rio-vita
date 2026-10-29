#include "gangstar_jni_strings.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <stdlib.h>
static atomic_uint allocations,frees;
static atomic_int fail_next;
void *__real_calloc(size_t,size_t);
void __real_free(void *);
void *__wrap_calloc(size_t n,size_t z){if(atomic_exchange(&fail_next,0))return NULL;void *p=__real_calloc(n,z);if(p)++allocations;return p;}
void __wrap_free(void *p){if(p)++frees;__real_free(p);}
static struct JNINativeInterface t;
static unsigned foreign_local,foreign_global;
static uintptr_t thread_id(void){return (uintptr_t)pthread_self();}
static jobject identity(JNIEnv *e,jobject p){(void)e;return p;}
static void old_local(JNIEnv *e,jobject p){(void)e;assert(p);++foreign_local;}
static void old_global(JNIEnv *e,jobject p){(void)e;assert(p);++foreign_global;}
static void empty(void){RioJniStringStats s=gangstar_jni_strings_stats();assert(s.live==0 && s.bytes==0 && s.globals==0 && s.pins==0 && s.created==s.destroyed);assert(atomic_load(&allocations)==atomic_load(&frees));}
static void *worker(void *unused){(void)unused;for(unsigned i=0;i<10000;++i){jstring s=t.NewStringUTF(NULL,"parallel");assert(s);t.DeleteLocalRef(NULL,s);}gangstar_jni_strings_frame_end();return NULL;}
int main(void) {
 t.NewGlobalRef=identity;t.NewLocalRef=identity;t.DeleteLocalRef=old_local;t.DeleteGlobalRef=old_global;
 gangstar_jni_strings_install(&t,thread_id);
 for(unsigned frame=0;frame<10000;++frame) {
  for(unsigned i=0;i<10;++i) {
   jstring s=t.NewStringUTF(NULL,"world/texture.tga");assert(s);jboolean copy=JNI_TRUE;
   const char *p=t.GetStringUTFChars(NULL,s,&copy);assert(p && !copy && !strcmp(p,"world/texture.tga"));
   t.ReleaseStringUTFChars(NULL,s,(char *)p);
   if(i&1)t.DeleteLocalRef(NULL,s);
  }
  gangstar_jni_strings_frame_end();empty();
 }
 jstring s=t.NewStringUTF(NULL,"retained");assert(t.NewGlobalRef(NULL,s)==s);
 t.DeleteLocalRef(NULL,s);gangstar_jni_strings_frame_end();assert(gangstar_jni_strings_stats().live==1);
 for(unsigned i=0;i<1000;++i) {
  assert(t.PushLocalFrame(NULL,16)==JNI_OK);assert(t.NewLocalRef(NULL,s)==s);
  assert(t.PopLocalFrame(NULL,NULL)==NULL);
 }
 const char *p=t.GetStringUTFChars(NULL,s,NULL);assert(!strcmp(p,"retained"));
 t.DeleteGlobalRef(NULL,s);assert(gangstar_jni_strings_stats().live==1);
 t.ReleaseStringUTFChars(NULL,s,(char *)p);empty();
 // A borrowed view pins its object even after its local reference is deleted.
 s=t.NewStringUTF(NULL,"pinned");p=t.GetStringUTFChars(NULL,s,NULL);
 t.DeleteLocalRef(NULL,s);gangstar_jni_strings_frame_end();assert(!strcmp(p,"pinned"));
 assert(gangstar_jni_strings_stats().pins==1);t.ReleaseStringUTFChars(NULL,s,(char *)p);empty();
 // PopLocalFrame promotes only the result; parent locals survive the pop.
 jstring parent=t.NewStringUTF(NULL,"parent");assert(t.PushLocalFrame(NULL,2)==JNI_OK);
 s=t.NewStringUTF(NULL,"result");assert(t.NewStringUTF(NULL,"temporary"));
 assert(t.PopLocalFrame(NULL,s)==s && gangstar_jni_strings_stats().live==2);
 assert(t.GetStringLength(NULL,parent)==6 && t.GetStringLength(NULL,s)==6);
 gangstar_jni_strings_frame_end();empty();
 // UTF-16 NUL and surrogate pairs are preserved in modified UTF-8.
 const jchar wide[]={0x41,0,0xe7,0xd83d,0xde00};
 s=t.NewString(NULL,wide,5);assert(s && t.GetStringLength(NULL,s)==5 && t.GetStringUTFLength(NULL,s)==11);
 p=t.GetStringUTFChars(NULL,s,NULL);assert((unsigned char)p[1]==0xc0 && (unsigned char)p[2]==0x80);
 jstring round=t.NewStringUTF(NULL,p);const jchar *w=t.GetStringChars(NULL,round,NULL);
 assert(t.GetStringLength(NULL,round)==5 && !memcmp(w,wide,sizeof(wide)));
 t.ReleaseStringChars(NULL,round,w);t.ReleaseStringUTFChars(NULL,s,(char *)p);
 char region[3]={7,7,7};t.GetStringUTFRegion(NULL,s,1,1,region);assert((unsigned char)region[0]==0xc0 && (unsigned char)region[1]==0x80 && region[2]==7);
 jchar region_w[2]={0};t.GetStringRegion(NULL,s,3,2,region_w);assert(region_w[0]==0xd83d && region_w[1]==0xde00);
 gangstar_jni_strings_frame_end();empty();
 s=t.NewStringUTF(NULL,"");assert(s && t.GetStringLength(NULL,s)==0);
 assert(!t.NewString(NULL,NULL,-1));assert(!t.NewString(NULL,NULL,1));
 gangstar_jni_strings_frame_end();empty();
 atomic_store(&fail_next,1);assert(!t.NewStringUTF(NULL,"out of memory"));empty();
 s=t.NewStringUTF(NULL,"kept");assert(t.NewGlobalRef(NULL,s)==s);
 assert(t.PushLocalFrame(NULL,1)==JNI_OK);atomic_store(&fail_next,1);
 assert(!t.NewLocalRef(NULL,s));t.PopLocalFrame(NULL,NULL);
 t.DeleteGlobalRef(NULL,s);gangstar_jni_strings_frame_end();empty();
 int foreign=1;t.DeleteLocalRef(NULL,&foreign);t.DeleteGlobalRef(NULL,&foreign);
 assert(foreign_local==1 && foreign_global==1);
 pthread_t a,b;assert(!pthread_create(&a,NULL,worker,NULL));assert(!pthread_create(&b,NULL,worker,NULL));
 assert(!pthread_join(a,NULL));assert(!pthread_join(b,NULL));empty();
 // Reproduce AndroidGetDeviceModel: repeated fixed callback response and
 // borrowed UTF pointer, deliberately without ReleaseStringUTFChars.
 s=gangstar_jni_string_constant("PS Vita");assert(s);
 p=t.GetStringUTFChars(NULL,s,NULL);gangstar_jni_strings_frame_end();
 RioJniStringStats before=gangstar_jni_strings_stats();
 for(unsigned frame=0;frame<10000;++frame) {
  for(unsigned i=0;i<100;++i) {
   jstring same=gangstar_jni_string_constant("PS Vita");assert(same==s);
   assert(t.GetStringUTFChars(NULL,same,NULL)==p);
  }
  gangstar_jni_strings_frame_end();
 }
 RioJniStringStats after=gangstar_jni_strings_stats();
 assert(after.live==1 && after.created==before.created && after.bytes==before.bytes);
 assert(after.pins==0 && after.globals==1 && !strcmp(p,"PS Vita"));
 t.DeleteGlobalRef(NULL,s);gangstar_jni_strings_frame_end();
 assert(gangstar_jni_strings_stats().live==1 && !strcmp(p,"PS Vita"));
 assert(atomic_load(&allocations)-atomic_load(&frees)==1);
 puts("JNI strings: 10000 frames without growth, local/global refs, scoped promotion, pinned views, UTF encodings and concurrent ownership passed");
}
