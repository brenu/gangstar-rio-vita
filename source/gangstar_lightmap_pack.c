#include "gangstar_lightmap_pack.h"
#include "gangstar_storage_trace.h"
#ifdef GANGSTAR_LIGHTMAP_PACK
#include "gangstar_diagnostics.h"
#include "utils/logger.h"
#include "gangstar_district_manifest.h"
#include "lz4.h"
#include <zlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#ifdef __vita__
#include <malloc.h>
#endif
// Private cache: header RLC1/main OBB size/count/record size, then 20-byte
// little-endian records (original offset/size, packed offset/size, CRC32).
static struct {uint32_t offset,size,position,packed,crc;atomic_int bad;} entries[128];
static unsigned count;
static int pack_fd=-1;
static pthread_once_t once=PTHREAD_ONCE_INIT;
static uint32_t u32(const unsigned char *p) {
 return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static int exact(int fd,void *out,size_t length,uint32_t offset) {
 size_t done=0;
 while(done<length) {
#ifdef GANGSTAR_STORAGE_TRACE
  struct RioStorageSample sample=rio_storage_begin();
#endif
  ssize_t n=pread(fd,(char *)out+done,length-done,(off_t)offset+done);
#ifdef GANGSTAR_STORAGE_TRACE
  rio_storage_end(sample,2,(uint64_t)offset+done,length-done,(int)n);
#endif
  if(n<0 && errno==EINTR)continue;
  if(n<=0 || (size_t)n>length-done)return 0;
  done+=(size_t)n;
 }
 return 1;
}
static int supported(uint32_t offset,uint32_t size) {
 for(unsigned i=0;i<sizeof(rio_district_assets)/sizeof(*rio_district_assets);++i)
  if(rio_district_assets[i].offset==offset && rio_district_assets[i].size==size &&
     strstr(rio_district_assets[i].name,"_lightmap"))return 1;
 return 0;
}
static void initialize(void) {
 unsigned char header[20];struct stat st;
 int fd=open(DATA_PATH "lightmaps.lz4",O_RDONLY);
 if(fd<0)return;
 int valid=!fstat(fd,&st) && st.st_size>=16 && st.st_size<=INT32_MAX &&
  exact(fd,header,16,0) && !memcmp(header,"RLC1",4) && u32(header+4)==2119380688u && u32(header+12)==20;
 unsigned n=valid?u32(header+8):0;
 if(!n || n>128)valid=0;
 uint64_t end=16u+(uint64_t)n*20u;
 for(unsigned i=0;valid && i<n;++i) {
  if(!exact(fd,header,20,16+i*20)) {valid=0;break;}
  uint32_t offset=u32(header),size=u32(header+4),position=u32(header+8),packed=u32(header+12);
  if(!supported(offset,size) || position!=end || !packed || packed>=size || size>2796344u ||
     (uint64_t)position+packed>(uint64_t)st.st_size) {valid=0;break;}
  for(unsigned j=0;j<i;++j)if(entries[j].offset==offset)valid=0;
  entries[i].offset=offset;entries[i].size=size;entries[i].position=position;
  entries[i].packed=packed;entries[i].crc=u32(header+16);end+=packed;
 }
 if(valid && end==(uint64_t)st.st_size) {count=n;pack_fd=fd;l_info("RIO_LIGHTMAP_PACK ready=1 entries=%u",n);}
 else {close(fd);l_warn("RIO_LIGHTMAP_PACK invalid_index=1 fallback=original_obb");}
}
int gangstar_lightmap_pack_read(uint32_t offset,uint32_t size,void *destination) {
 int saved=errno;int result=0;
 if(!destination)return 0;
 pthread_once(&once,initialize);
 if(pack_fd<0) {errno=saved;return 0;}
 for(unsigned i=0;i<count;++i)if(entries[i].offset==offset && entries[i].size==size) {
  if(atomic_load(&entries[i].bad))break;
#ifdef __vita__
  struct mallinfo heap=mallinfo();
  if((uint64_t)(unsigned)heap.uordblks+entries[i].packed>192u*1024u*1024u)break;
#endif
  void *packed=malloc(entries[i].packed);if(!packed)break;
  uint64_t began=rio_diagnostic_clock();
  if(!exact(pack_fd,packed,entries[i].packed,entries[i].position))goto invalid;
  uint64_t read_done=rio_diagnostic_clock();
  if(LZ4_decompress_safe(packed,destination,(int)entries[i].packed,(int)size)!=(int)size)goto invalid;
  uint64_t decoded=rio_diagnostic_clock();
  if(crc32(0,destination,size)!=entries[i].crc)goto invalid;
  uint64_t verified=rio_diagnostic_clock();
  (void)began;(void)read_done;(void)decoded;(void)verified;
  l_info("RIO_LIGHTMAP_PACK offset=%u bytes=%u packed=%u read_us=%llu decode_us=%llu verify_us=%llu total_us=%llu hit=1",
   offset,size,entries[i].packed,(unsigned long long)(read_done-began),(unsigned long long)(decoded-read_done),
   (unsigned long long)(verified-decoded),(unsigned long long)(verified-began));
  result=1;free(packed);break;
invalid:
  free(packed);atomic_store(&entries[i].bad,1);
  l_warn("RIO_LIGHTMAP_PACK invalid_payload=1 offset=%u fallback=original_obb",offset);break;
 }
 errno=saved;return result;
}
#endif
