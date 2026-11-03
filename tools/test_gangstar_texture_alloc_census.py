"""Run the actual texture attribution code against known allocation metadata."""
from pathlib import Path
import os,subprocess
s=Path('lib/vitagl/source/textures.c').read_text()
a=s.index('void gangstar_texture_alloc_census(');b=s.index('\n#endif\nsampler *',a)
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define TEXTURES_NUM 2060
#define TEX_VALID 1
#define VGL_MEM_VRAM 0
#define VGL_MEM_EXTERNAL 4
#define SCE_GXM_TEXTURE_FORMAT_ETC1_1BGR 9
#define OBJ_NOT_USED 0xffffffffu
#define OBJ_CACHED 0xfffffffeu
typedef struct {unsigned w,h,format;} Descriptor;
typedef struct {int status;void *data;unsigned overridden,dirty,ref_counter,last_frame,mip_count,compressed_mip_failed;Descriptor gxm_tex;} texture;
static texture texture_slots[TEXTURES_NUM];
static unsigned vgl_framecount=1000,queries,records,bulk,large_records;
static size_t sizes[8];static unsigned record_flags[8],record_ages[8];
static int vgl_mem_get_type_by_addr(void *p){
 if(bulk)return 0;
 switch((uintptr_t)p){case 0x1000:return 0;case 0x2000:return 1;case 0x3000:return 4;case 0x6000:return 2;default:return -1;}
}
static size_t vgl_malloc_usable_size(void *p){
 ++queries;if(bulk)return 16;
 assert(p!=(void *)0x4000 && p!=(void *)0x5000);
 return (uintptr_t)p/0x1000*100;
}
static void vglGetTexSizes(const Descriptor *d,unsigned *w,unsigned *h){*w=d->w;*h=d->h;}
static unsigned vglGetTexFormat(const Descriptor *d){return d->format;}
void gangstar_texture_live_entry(unsigned slot,const void *,const void *p,size_t bytes,unsigned w,unsigned h,unsigned format,unsigned mips,int pool,unsigned age,unsigned flags) __attribute__((weak));
void gangstar_texture_live_entry(unsigned slot,const void *object,const void *p,size_t bytes,unsigned w,unsigned h,unsigned format,unsigned mips,int pool,unsigned age,unsigned flags){
 assert(slot<TEXTURES_NUM && object==&texture_slots[slot] && p && w && h && format && mips && pool>=0 && pool<=4 && records<8);
 sizes[records]=bytes;record_flags[records]=flags;record_ages[records++]=age;
}
void gangstar_texture_large_entry(unsigned slot,const void *object,const void *p,size_t bytes,unsigned w,unsigned h,unsigned format,unsigned mips,int pool,unsigned age,unsigned flags){
 assert((bulk || slot==1 || slot==8) && object==&texture_slots[slot] && p && bytes && w==2048 && h==2048 && format==9 && mips==5 && (pool==0 || pool==2) && age==900 && !flags);++large_records;
}
''' + s[a:b] + r'''
static void setup(unsigned i,uintptr_t data,unsigned edge,unsigned format){
 texture_slots[i]=(texture){.status=TEX_VALID,.data=(void *)data,.last_frame=100,.mip_count=5,.gxm_tex={edge,edge,format}};
}
int main(void){
 setup(1,0x1000,2048,9);setup(2,0x2000,512,9);texture_slots[2].last_frame=OBJ_NOT_USED;
 setup(3,0x3000,256,8);texture_slots[3].dirty=texture_slots[3].ref_counter=texture_slots[3].compressed_mip_failed=1;
 setup(4,0x1000,2048,9); // Alias must not double-count bytes.
 setup(5,0x4000,2048,9);texture_slots[5].overridden=1;
 setup(6,0x5000,2048,9); // Unknown pointer: do not read allocator headers.
 setup(7,0,2048,9);setup(8,0x6000,2048,9);
 texture before[TEXTURES_NUM];memcpy(before,texture_slots,sizeof(before));
 uint64_t out[12],pools[5];gangstar_texture_alloc_census(out,pools);
 assert(out[0]==6 && out[1]==4 && out[2]==1200 && out[3]==900 && out[4]==2 && out[5]==700 && out[6]==300);
 assert(out[7]==1 && out[8]==1 && out[9]==1 && !out[10] && out[11]==1);
 assert(pools[0]==100 && pools[1]==200 && pools[2]==600 && !pools[3] && pools[4]==300 && queries==4);
 assert(records==4 && sizes[0]==600 && sizes[1]==300 && sizes[2]==200 && sizes[3]==100);
 assert(large_records==2);
 assert(record_ages[0]==900 && record_ages[2]==0xffffffffu && record_flags[2]&1 && record_flags[1]==22);
 assert(!memcmp(before,texture_slots,sizeof(before)));
 records=0;gangstar_texture_alloc_census(out,pools);assert(out[2]==1200 && queries==8);
 assert(large_records==4);
 memset(texture_slots,0,sizeof(texture_slots));bulk=1;records=0;
 for(unsigned i=0;i<TEXTURES_NUM;++i)setup(i,(i+1)*16,64,9);
 gangstar_texture_alloc_census(out,pools);
 assert(out[1]==2048 && out[10]==12 && out[2]==2048*16 && !out[9] && !out[11] && records==8);
 records=large_records=0;
 for(unsigned i=0;i<TEXTURES_NUM;++i)setup(i,(i+1)*16,2048,9);
 gangstar_texture_alloc_census(out,pools);
 assert(records==8 && large_records==2048); // Complete large set, not just top eight.
}
'''
p=Path('work/tests/texture_alloc_census.c');p.parent.mkdir(parents=True,exist_ok=True);p.write_text(code)
subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-address','-DVGL_ETC1_OOM_FALLBACK','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p),'-o','work/tests/texture_alloc_census'],check=True)
subprocess.run(['work/tests/texture_alloc_census'],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
print('Texture allocations: production pool/format attribution, aliases, borrowed/unknown pointers, top allocations, usage age, hash saturation and unchanged textures passed (ASan/UBSan)')
