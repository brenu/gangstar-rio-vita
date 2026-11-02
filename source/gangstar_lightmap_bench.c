// Optional startup benchmark only. Private inputs are generated from user OBBs.
#ifdef GANGSTAR_LIGHTMAP_BENCH
#include "utils/logger.h"
#include <psp2/kernel/processmgr.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <zlib.h>
#include "lz4.h"
static uint32_t bench_u32(const unsigned char *p) {
 return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static int bench_read(int fd,void *out,size_t length,uint32_t offset) {
 size_t done=0;
 while(done<length) {
  ssize_t n=pread(fd,(char *)out+done,length-done,(off_t)offset+done);
  if(n<0 && errno==EINTR)continue;
  if(n<=0 || (size_t)n>length-done)return 0;
  done+=(size_t)n;
 }
 return 1;
}
void gangstar_lightmap_benchmark(void) {
 int pack=open(DATA_PATH "lightmap-benchmark.bin",O_RDONLY);
 if(pack<0)return;
 int raw=open(DATA_PATH "main.12030.com.gameloft.android.ANMP.GloftG4HM.obb",O_RDONLY);
 unsigned char header[16];void *source=NULL,*compressed=NULL,*decoded=NULL;
 int valid=0;
 if(raw<0 || !bench_read(pack,header,8,0) ||
    (memcmp(header,"RLB1",4) && memcmp(header,"RLB2",4)) || bench_u32(header+4)!=2)goto finish;
 int lz4=header[3]=='2';
 uint32_t position=8;
 for(unsigned entry=0;entry<2;++entry) {
  if(!bench_read(pack,header,16,position))goto finish;
  uint32_t offset=bench_u32(header),size=bench_u32(header+4),packed=bench_u32(header+8),crc=bench_u32(header+12);
  position+=16;
  // These two immutable original entries bound all offsets and allocations.
  if(offset!=(entry?806291654u:183416410u) || size!=2796344u || !packed || packed>size)goto finish;
  source=malloc(size);decoded=malloc(size);compressed=malloc(packed);
  if(!source || !decoded || !compressed)goto finish;
  for(unsigned pass=0;pass<3;++pass) {
   uint64_t raw_us=0,packed_us=0,decode_us=0;
   // Alternate ordering; later passes are warm comparisons, not cold-I/O claims.
   for(unsigned step=0;step<2;++step) {
    uint64_t began=sceKernelGetProcessTimeWide();
    if((step+pass)%2==0) {
     if(!bench_read(raw,source,size,offset))goto finish;
     raw_us=sceKernelGetProcessTimeWide()-began;
    } else {
     if(!bench_read(pack,compressed,packed,position))goto finish;
     packed_us=sceKernelGetProcessTimeWide()-began;
     uLongf output=size;began=sceKernelGetProcessTimeWide();
     if(lz4) {
      if(LZ4_decompress_safe(compressed,decoded,(int)packed,(int)size)!=(int)size)goto finish;
     } else if(uncompress(decoded,&output,compressed,packed)!=Z_OK || output!=size)goto finish;
     decode_us=sceKernelGetProcessTimeWide()-began;
    }
   }
   if(crc32(0,source,size)!=crc || memcmp(source,decoded,size))goto finish;
   l_info("RIO_LIGHTMAP_BENCH codec=%s entry=%u pass=%u raw_bytes=%u packed_bytes=%u raw_us=%llu packed_read_us=%llu decode_us=%llu packed_total_us=%llu exact=1",
    lz4?"lz4":"zlib",entry,pass,size,packed,(unsigned long long)raw_us,(unsigned long long)packed_us,
    (unsigned long long)decode_us,(unsigned long long)(packed_us+decode_us));
  }
  free(source);free(decoded);free(compressed);source=decoded=compressed=NULL;
  position+=packed;
 }
 valid=1;
finish:
 free(source);free(decoded);free(compressed);
 if(raw>=0)close(raw);
 close(pack);
 l_info("RIO_LIGHTMAP_BENCH finished=%d gameplay_path_unchanged=1",valid);
}
#endif
