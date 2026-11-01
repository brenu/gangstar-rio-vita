"""Check release hooks retain fixes and diagnostic hot paths do no clock work."""
from pathlib import Path
import subprocess,os
root=Path('work/tests');root.mkdir(parents=True,exist_ok=True)
def run(name,code,extra=()):
 p=root/(name+'.c');p.write_text(code)
 subprocess.run(['cc','-std=gnu11','-O2','-Wall','-Wextra','-Werror','-DGANGSTAR_RELEASE_BUILD','-Itools/test_mocks','-Isource',str(p),*extra,'-o',str(root/name)],check=True)
 subprocess.run([str(root/name)],check=True)
s=Path('source/gangstar_native_startup.c').read_text();a=s.index('static int native_probe_needed(');b=s.index('// Borrow only while',a)
for base in (47,50):
 run('release_gate', '#include <assert.h>\n#define RIO_PHASE_NATIVE_TID 46\n#define RIO_PHASE_SUBMIT_BASE 47\n#define PHASE_BASE %d\n'%base+('#define GANGSTAR_NATIVE_IMA_FAST\n' if base==50 else '')+s[a:b]+r'''
int main(void){
 for(unsigned i=0;i<PHASE_BASE+57;++i){
  int expected=i==14||i==15||i==16||i==17||i==20||i==21||(i>=33&&i<=39)||i==42;
  if(PHASE_BASE==50 && i==47)expected=1;
  if(i>=PHASE_BASE)expected=i==PHASE_BASE+46||i==PHASE_BASE+50||i==PHASE_BASE+54||i==PHASE_BASE+55||i==PHASE_BASE+56;
  assert(native_probe_needed(i)==expected);
 }
}
''')
run('release_wait',r'''
#include "gangstar_diagnostics.h"
#include "gangstar_wait_perf.h"
#include <assert.h>
#include <stdarg.h>
static int thread=1;
int sceKernelGetThreadId(void){return thread;}
uint64_t sceKernelGetProcessTimeWide(void){assert(!"diagnostic clock invoked");return 0;}
void _log_print(int kind,const char *fmt,...){(void)kind;(void)fmt;}
int main(void){
 assert(!rio_diagnostic_clock());gangstar_wait_frame();assert(gangstar_wait_is_main());
 for(int i=0;i<100000;++i)assert(!gangstar_wait_begin());
 thread=2;assert(!gangstar_wait_is_main());
}
''',('source/gangstar_wait_perf.c',))
run('release_audio_scope',r'''
#include "gangstar_audio_probe.h"
#include "gangstar_audio_chunk.h"
#include <assert.h>
int main(void){
 RioAudioProbeScope outer,inner;assert(!rio_audio_scope_active());
 rio_audio_scope_begin(&outer,123,1,0);assert(rio_audio_scope_active() && rio_audio_scope_matches(123));
 assert(rio_audio_half_chunk(25600,4,1,rio_audio_scope_matches(123))==12800);
 rio_audio_scope_begin(&inner,456,1,0);assert(rio_audio_scope_matches(456));
 rio_audio_scope_end(&inner,0);assert(rio_audio_scope_matches(123));
 rio_audio_scope_end(&outer,0);assert(!rio_audio_scope_active());
}
''',('source/gangstar_audio_probe.c','-pthread'))
# The DDS hook used to gate pinning on a nonzero profiling timestamp.
s=Path('source/gangstar_native_startup.c').read_text();a=s.index('static uintptr_t probe_21(');b=s.index('\n#ifdef GANGSTAR_RESIDENT_TEXTURES',a)
run('release_texture_pin',r"""
#include <stdint.h>
#include <assert.h>
#define GANGSTAR_TEXTURE_REF_CACHE
struct {uintptr_t (*original)(uintptr_t,uintptr_t,uintptr_t,uintptr_t);} probes[22];
static unsigned pins,calls;
static int main_thread=1;
static uintptr_t table[11],file[1],texture=77;
static int gangstar_wait_is_main(void){return main_thread;}
static unsigned size(uintptr_t p){assert(p==(uintptr_t)file);return 2796344;}
static const char *name(uintptr_t p){assert(p==(uintptr_t)file);return "map.tga";}
static void pin_texture(uintptr_t object,const char *n,unsigned bytes){assert(object==77 && n && bytes==2796344);++pins;}
static uintptr_t original(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d){assert(a==11 && b==(uintptr_t)file && c==(uintptr_t)&texture && d==44);++calls;return 123;}
"""+s[a:b]+r"""
int main(void){
 table[8]=(uintptr_t)size;table[10]=(uintptr_t)name;file[0]=(uintptr_t)table;probes[21].original=original;
 assert(probe_21(11,(uintptr_t)file,(uintptr_t)&texture,44)==123);assert(pins==1 && calls==1);
 main_thread=0;assert(probe_21(11,(uintptr_t)file,(uintptr_t)&texture,44)==123);assert(pins==1 && calls==2);
}
""")
s=Path('source/patch.c').read_text();a=s.index('static void gangstar_audio_callback(');b=s.index('void gangstar_audio_report(',a)
run('release_audio_callback',r'''
#include <assert.h>
#include <stdint.h>
static unsigned order;
static unsigned char driver[256];
static void gangstar_vox_lock(void *p){assert(p==driver+4 && order++==0);}
static void gangstar_vox_fill(void *p){assert(p==driver && order++==1);}
static void gangstar_vox_unlock(void *p){assert(p==driver+4 && order++==2);}
'''+s[a:b]+r'''
int main(void){gangstar_audio_callback(0,0);assert(!order);gangstar_audio_callback(0,driver);assert(order==3);}
''')
s=Path('tools/test_gangstar_asset_perf.c').read_text();s=s[:s.index('int main(void)')]
s=s.replace('now += 100; return now;','assert(!"asset diagnostic clock invoked");return now;')
run('release_assets',s+r'''
int main(void){
 gangstar_asset_frame();
 gangstar_tex_image(1,2,3,4,5,0,6,7,input);
 gangstar_tex_sub_image(1,2,3,4,5,6,7,8,input);
 gangstar_compressed_tex_image(1,2,3,4,5,0,sizeof(input),input);
 gangstar_generate_mipmap(1);assert(texture_calls==4);
 assert(!gangstar_upload_clock());
 GangstarAssetPerf sample=gangstar_asset_stats();assert(!sample.calls[RIO_ASSET_TEXTURE]);
}
''',('source/gangstar_asset_perf.c','-lz'))
print('Release: functional hook allowlist, no diagnostic clocks, worker exclusion, audio chunk scope, exact lock/fill/unlock and unchanged GL forwarding passed')
