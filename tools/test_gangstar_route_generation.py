"""Exercise actual mutation wrappers that invalidate direct archive routes."""
from pathlib import Path
import subprocess
src=Path('source/gangstar_native_startup.c').read_text()
start=src.index('static struct RouteMutation')
end=src.index('#define ROUTE_MUTATION',start)
s=r'''
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include <assert.h>
typedef uintptr_t (*NativeCall)(uintptr_t,uintptr_t,uintptr_t,uintptr_t);
static atomic_uint route_generation=1;
'''+src[start:end]+r'''
static unsigned calls;
static uintptr_t original(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d){
 assert(a==1 && b==2 && c==3 && d==4);
 assert(atomic_load(&route_generation)==2+calls*2);++calls;return 99;
}
int main(void){
 for(unsigned i=0;i<5;++i){
  route_mutations[i].original=original;
  assert(route_mutation(i,1,2,3,4)==99);
  assert(atomic_load(&route_generation)==3+i*2);
 }
 assert(calls==5);
}
'''
p=Path('work/tests/route_generation.c');p.write_text(s)
subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',str(p),'-o','work/tests/route_generation'],check=True)
subprocess.run(['work/tests/route_generation'],check=True)
print('Direct routes: every filesystem mutation/lifetime wrapper invalidates before and after while preserving arguments/results')
