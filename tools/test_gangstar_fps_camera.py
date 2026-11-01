"""Verify targeted FPS fallback against the actual native call sites and stack layout."""
from pathlib import Path
import struct,subprocess
s=Path('source/patch.c').read_text();a=s.index('static float *(*fps_scene_position)');b=s.index('static void gangstar_fps_camera_guard(',a)
e=Path('../work/libgl2jni.so').read_bytes()
for site,load,bl,target in ((0x506fa8,0xe28d0068,0xeb064636,0x506f74),(0x50743c,0xe28d0070,0xeb064511,0x5073f8)):
 assert struct.unpack_from('<2I',e,site-4)==(load,bl)
 # The earlier virtual getPosition writes exactly the following 12-byte vector.
 out_offset=load&0xfff
 assert struct.unpack_from('<I',e,target)[0]==0xe28d0000+(out_offset+12)
 imm=bl&0xffffff
 if imm&0x800000:imm-=0x1000000
 assert site+8+imm*4==0x698888
p=Path('work/tests/fps_camera.c')
p.write_text(r'''
#include <assert.h>
#include <string.h>
static unsigned logs,calls;
#define l_warn(...) (++logs)
'''+s[a:b]+r'''
static float *original(float *out,const void *node){++calls;memcpy(out,node,12);return out;}
int main(void){
 fps_scene_position=original;
 float native_stack[6]={-1,-1,-1,123.5f,-45.25f,901.75f};
 float node[3]={1,2,3};
 assert(fps_optional_position(native_stack,node)==native_stack);
 assert(calls==1 && !memcmp(native_stack,node,12) && !logs);
 for(unsigned i=0;i<100;++i){
  assert(fps_optional_position(native_stack,0)==native_stack);
  assert(!memcmp(native_stack,native_stack+3,12));
 }
 assert(calls==1 && logs==8);
 assert(fps_optional_position(native_stack,node)==native_stack);
 assert(calls==2 && !memcmp(native_stack,node,12));
}
''')
subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',str(p),'-o','work/tests/fps_camera'],check=True)
subprocess.run(['work/tests/fps_camera'],check=True)
print('FPS camera: actual ELF call sites and adjacent target-position stack vectors verified; valid nodes unchanged, null nodes use target position, logging bounded')
