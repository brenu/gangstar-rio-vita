"""Exercise the actual mode-1 selective-copy index scan and overflow gates."""
from pathlib import Path
import subprocess
s=Path('lib/vitagl/source/custom_shaders.c').read_text()
a=s.index('  if(!top_idx) {',s.index('#else // DRAW_SPEEDHACK',s.index('GLboolean _glDrawElements_CustomShadersRangeIMPL')))
b=s.index('  if(rio_scan &&',a)
assert 'rio_index_max16' in s[a:b] and 'rio_index_max32' in s[a:b]
prefix='''
#include "index_max.h"
#include <assert.h>
#define GL_FALSE 0
static uint64_t rio_index_scan_calls[2],rio_index_scan_elements[2];
static int scan(uint16_t *idx_buf,int count,unsigned index_type,uint32_t base_idx,uint32_t top_idx,uint32_t *out) {
'''
main='''
 *out=top_idx;return 1;
}
int main(void) {
 uint16_t a[]={0,65535,1};uint32_t b[]={0,70000,UINT32_MAX},out=99;
 assert(scan(a,3,0,0,0,&out) && out==65536);
 assert(scan((uint16_t *)b,2,1,5,0,&out) && out==70006);
 assert(!scan((uint16_t *)b,3,1,0,0,&out));
 assert(!scan(a,3,0,UINT32_MAX-1,0,&out));
 assert(!scan(NULL,0,0,0,0,&out));
 assert(!scan(NULL,-1,0,0,0,&out));
 uint64_t before=rio_index_scan_calls[0];
 assert(scan(NULL,3,0,0,123,&out) && out==123 && rio_index_scan_calls[0]==before);
 assert(rio_index_scan_calls[0]==2 && rio_index_scan_calls[1]==2);
 assert(rio_index_scan_elements[0]==6 && rio_index_scan_elements[1]==5);
}
'''
p=Path('work/tests/active_index_scan.c');p.write_text(prefix+s[a:b]+main)
subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-I','lib/vitagl/source/utils',str(p),'-o','work/tests/active_index_scan'],check=True)
subprocess.run(['work/tests/active_index_scan'],check=True)
print('Mode-1 actual scan: both typed scans invoked/counted, known bounds bypass scanning, zero/negative counts and U32/base overflow rejected')
