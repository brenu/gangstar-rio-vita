"""Exercise the actual native wrapper with mock weapon lookups/swaps."""
from pathlib import Path
import subprocess
src=Path('source/gangstar_native_startup.c').read_text()
start=src.index('static uintptr_t (*weapon_current)')
end=src.index('static void install_weapon_guard',start)
test=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#define l_warn(...) ((void)0)
'''+src[start:end]+r'''
static unsigned current_calls,list_calls,swaps;
static uintptr_t result;
static uintptr_t current(uintptr_t m,uintptr_t i){assert(m==7 && i);++current_calls;return result;}
static uintptr_t list(uintptr_t m,int n,uintptr_t i){assert(m==7 && n==36 && i);++list_calls;return result;}
static void swap(uintptr_t m,uintptr_t c,uintptr_t i){assert(m==7 && c==9 && i);++swaps;}
int main(void){
 unsigned char info[0x140]={0};int index=36;
 memcpy(info+0x124,&index,4);
 weapon_current=current;weapon_in_list=list;weapon_swap_original=swap;
 guarded_weapon_swap(7,9,0);assert(!swaps && !current_calls);
 guarded_weapon_swap(7,9,(uintptr_t)info);assert(!swaps && current_calls==1);
 result=123;guarded_weapon_swap(7,9,(uintptr_t)info);assert(swaps==1 && current_calls==2);
 info[0x128]=1;result=0;guarded_weapon_swap(7,9,(uintptr_t)info);assert(swaps==1 && list_calls==1);
 result=123;guarded_weapon_swap(7,9,(uintptr_t)info);assert(swaps==2 && list_calls==2);
 index=-1;memcpy(info+0x124,&index,4);guarded_weapon_swap(7,9,(uintptr_t)info);
 index=44;memcpy(info+0x124,&index,4);guarded_weapon_swap(7,9,(uintptr_t)info);
 assert(swaps==2 && list_calls==2 && current_calls==2);
}
'''
path=Path('work/tests/weapon_guard.c');path.write_text(test)
subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',str(path),'-o','work/tests/weapon_guard'],check=True)
subprocess.run(['work/tests/weapon_guard'],check=True)
print('Weapon guard: empty/missing weapons, invalid indices, both inventory modes and unchanged valid swaps passed')
