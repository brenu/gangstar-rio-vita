#include "gangstar_save_identity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define SAVE_LIMIT (2u*1024u*1024u)
#define PATH_CAP 768
static uint32_t u32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(i*8));}
static int valid_id(const char *s,size_t n){
 if(!n || n>=RIO_SAVE_ID_CAP)return 0;
 for(size_t i=0;i<n;++i)if((unsigned char)s[i]<33 || (unsigned char)s[i]>126)return 0;
 return 1;
}
/* Native Encrypt/Decrypt use independent TEA blocks with the 16-byte key stored
 * in the version-3 file header. This is format decoding, not a save rewrite. */
static void decrypt(uint8_t *data,size_t length,const uint8_t *key){
 uint32_t k[4];for(unsigned i=0;i<4;++i)k[i]=u32(key+i*4);
 for(size_t off=0;off<length;off+=8){
  uint32_t a=u32(data+off),b=u32(data+off+4),sum=0xc6ef3720u;
  for(unsigned i=0;i<32;++i){
   b-=((a<<4)+k[2])^((a>>5)+k[3])^(a+sum);
   a-=((b<<4)+k[0])^((b>>5)+k[1])^(b+sum);sum-=0x9e3779b9u;
  }
  put32(data+off,a);put32(data+off+4,b);
 }
}
static uint32_t native_hash(const uint8_t *data,size_t size){
 uint32_t table[256],seed=0x100001u;
 for(unsigned i=0;i<256;++i)for(unsigned j=0;j<5;++j){
  seed=(seed*125u+3u)%0x2aaaabu;uint32_t high=(seed&65535u)<<16;
  seed=(seed*125u+3u)%0x2aaaabu;if(!j)table[i]=high|(seed&65535u);
 }
 uint32_t a=0x7fed7fedu,b=0xeeeeeeeeu;
 for(size_t i=0;i<size;++i){a=table[data[i]]^(a+b);b=data[i]+a+b+(b<<5)+3u;}
 return a;
}
static int validate(const uint8_t *data,size_t size,unsigned kind,char *identity){
 if(!data || size<52 || size>SAVE_LIMIT || u32(data)!=0xfffffffdu ||
    u32(data+4)!=32 || u32(data+8)!=0x01010530u || u32(data+12)!=kind ||
    (kind==1 && (size-40)%8))return 0;
 uint8_t *copy=malloc(size);if(!copy)return 0;memcpy(copy,data,size);
 if(kind==1)decrypt(copy+40,size-40,copy+24);
 char found[RIO_SAVE_ID_CAP]={0};int seen=0,valid=0;size_t pos=40;
 while(size-pos>=8){
  uint32_t tag=u32(copy+pos),n=u32(copy+pos+4);
  if(n>size-pos-8)break;
  const uint8_t *payload=copy+pos+8;
  if(tag==0x44465620u){
   if(seen++ || n<4 || u32(payload)!=n-4 || !valid_id((const char *)payload+4,n-4))break;
   memcpy(found,payload+4,n-4);
  }
  if(tag==0x454e4458u){
   size_t end=pos+8+n,padding=kind==1?((8-(end-40)%8)%8):0;
   valid=n==4 && end+padding==size &&
    (kind==1 ? (seen==1 && u32(payload)==native_hash(copy,pos)) : u32(payload)==UINT32_MAX);
   break;
  }
  pos+=8+n;
 }
 if(valid && identity)memcpy(identity,found,sizeof(found));
 free(copy);return valid;
}
int gangstar_save_read_identity(const uint8_t *data,size_t size,char identity[RIO_SAVE_ID_CAP]){
 if(!identity)return 0;
 identity[0]=0;return validate(data,size,1,identity);
}

typedef struct {const char *name;uint8_t *data;size_t size;int present;} File;
static int fail(char *error,size_t n,const char *message){if(error && n)snprintf(error,n,"%s",message);return 0;}
static int path_join(char *out,const char *root,const char *name){
 int n=snprintf(out,PATH_CAP,"%s%s%s",root,root[0] && root[strlen(root)-1]!='/'?"/":"",name);
 return n>=0 && n<PATH_CAP;
}
static int load(const char *path,File *out){
 FILE *f=fopen(path,"rb");if(!f)return errno==ENOENT;
 struct stat st;if(fstat(fileno(f),&st) || !S_ISREG(st.st_mode) || st.st_size<0 || (uint64_t)st.st_size>SAVE_LIMIT){fclose(f);return 0;}
 size_t n=(size_t)st.st_size;uint8_t *data=malloc(n+1);if(!data){fclose(f);return 0;}
 int ok=fread(data,1,n,f)==n && !ferror(f);if(fclose(f))ok=0;
 if(!ok){free(data);return 0;}data[n]=0;out->data=data;out->size=n;out->present=1;return 1;
}
static int same_file(const char *path,const File *expected){
 File actual={0};int ok=load(path,&actual) && actual.present==expected->present;
 if(ok && expected->present)ok=actual.size==expected->size && !memcmp(actual.data,expected->data,actual.size);
 free(actual.data);return ok;
}
static int write_new(const char *path,const void *data,size_t size){
 int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0)return 0;
 size_t done=0;int ok=1;
 while(done<size){ssize_t n=write(fd,(const uint8_t *)data+done,size-done);if(n<0 && errno==EINTR)continue;if(n<=0){ok=0;break;}done+=(size_t)n;}
 if(ok && fsync(fd))ok=0;
 if(close(fd))ok=0;
 return ok;
}
static int snapshot(const char *root,File *files,unsigned count){
 char base[PATH_CAP],dir[PATH_CAP],path[PATH_CAP];
 if(!path_join(base,root,"save-backups"))return 0;
 if(mkdir(base,0700) && errno!=EEXIST)return 0;
 uint64_t hash=UINT64_C(14695981039346656037);
 for(unsigned i=0;i<count;++i){
  hash=(hash^(unsigned)files[i].present)*UINT64_C(1099511628211);
  for(size_t j=0;files[i].name[j];++j)hash=(hash^(uint8_t)files[i].name[j])*UINT64_C(1099511628211);
  for(size_t j=0;j<files[i].size;++j)hash=(hash^files[i].data[j])*UINT64_C(1099511628211);
 }
 for(unsigned attempt=0;attempt<1024;++attempt){
  int n=snprintf(dir,sizeof(dir),"%s/%016llx-%u",base,(unsigned long long)hash,attempt);
  if(n<0 || (size_t)n>=sizeof(dir))return 0;
  if(mkdir(dir,0700)){
   if(errno!=EEXIST)return 0;
   int matches=1;for(unsigned i=0;i<count && matches;++i)matches=path_join(path,dir,files[i].name) && same_file(path,&files[i]);
   if(matches && path_join(path,dir,"complete") && access(path,F_OK)==0)return 1;
   continue; // Partial snapshot or collision: preserve it and choose a new directory.
  }
  for(unsigned i=0;i<count;++i)if(files[i].present){
   if(!path_join(path,dir,files[i].name) || !write_new(path,files[i].data,files[i].size) || !same_file(path,&files[i]))return 0;
  }
  return path_join(path,dir,"complete") && write_new(path,"verified\n",9);
 }
 return 0;
}
static int persist_id(const char *root,const char *id,const File *previous_id){
 char target[PATH_CAP],temporary[PATH_CAP],name[80],value[RIO_SAVE_ID_CAP+1];
 if(!path_join(target,root,"installation-id.txt"))return 0;
 int size=snprintf(value,sizeof(value),"%s\n",id);if(size<=0 || (size_t)size>=sizeof(value))return 0;
 for(unsigned i=0;i<1024;++i){
  snprintf(name,sizeof(name),"installation-id.txt.pending-%u",i);
  if(!path_join(temporary,root,name))return 0;
  if(access(temporary,F_OK)==0)continue;
  if(!write_new(temporary,value,(size_t)size))return 0;
  if(rename(temporary,target)){
   int error=errno;
   // Some Vita filesystems cannot rename over an existing destination. Move
   // the old ID aside without deleting it; a crash leaves recoverable bytes,
   // and the next startup can still adopt the identity embedded in the save.
   if((error!=EEXIST && error!=ENOTEMPTY) || !previous_id->present ||
      !same_file(target,previous_id))return 0;
   char held[PATH_CAP];
   snprintf(name,sizeof(name),"installation-id.txt.previous-%u",i);
   if(!path_join(held,root,name) || access(held,F_OK)==0 || rename(target,held))return 0;
   if(rename(temporary,target)){
    (void)rename(held,target); // Best-effort rollback; held copy survives failure.
    return 0;
   }
  }
  File expected={.data=(uint8_t *)value,.size=(size_t)size,.present=1};return same_file(target,&expected);
 }
 return 0;
}
int gangstar_save_prepare(const char *root,const char *fresh_id,char identity[RIO_SAVE_ID_CAP],char *error,size_t error_size){
 File files[]={{.name="Gangstar3.sav"},{.name="Missions.sav"},{.name="Gangstar3.preferences"},{.name="installation-id.txt"}};
 char path[PATH_CAP],stored[RIO_SAVE_ID_CAP]={0},selected[RIO_SAVE_ID_CAP]={0};int result=0;
 if(identity)identity[0]=0;
 if(error && error_size)error[0]=0;
 if(!root || !*root || !identity || !fresh_id || !valid_id(fresh_id,strlen(fresh_id)))return fail(error,error_size,"Invalid save identity configuration");
 for(unsigned i=0;i<4;++i){
  char relative[96];snprintf(relative,sizeof(relative),"%s%s",i<3?"saves/":"",files[i].name);
  if(!path_join(path,root,relative) || !load(path,&files[i])){fail(error,error_size,"Cannot safely read existing save files; startup stopped");goto done;}
 }
 if(files[0].present || files[1].present || files[2].present || files[3].present)
  if(!snapshot(root,files,4)){fail(error,error_size,"Cannot verify save backup; startup stopped without loading the game");goto done;}
 if(files[3].present){
  size_t n=files[3].size;while(n && (files[3].data[n-1]=='\n' || files[3].data[n-1]=='\r'))--n;
  if(valid_id((char *)files[3].data,n))memcpy(stored,files[3].data,n);
 }
 if(files[0].present || files[1].present){
  if(!files[0].present || !files[1].present){fail(error,error_size,"Incomplete save pair: keep Gangstar3.sav and Missions.sav together. Original files preserved");goto done;}
  if(!gangstar_save_read_identity(files[0].data,files[0].size,selected) || !validate(files[1].data,files[1].size,2,NULL)){
   fail(error,error_size,"Save is unsupported or damaged; original files and backup preserved. Startup stopped to prevent a reset");goto done;
  }
 }else snprintf(selected,sizeof(selected),"%s",*stored?stored:fresh_id);
 if(strcmp(selected,stored) && !persist_id(root,selected,&files[3])){fail(error,error_size,"Cannot persist the save's installation identity; originals preserved and startup stopped");goto done;}
 memcpy(identity,selected,sizeof(selected));result=1;
done:
 for(unsigned i=0;i<4;++i)free(files[i].data);
 return result;
}
