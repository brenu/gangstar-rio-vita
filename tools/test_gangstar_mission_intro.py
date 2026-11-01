"""Exercise live mission-intro restoration and background callback protection."""
from pathlib import Path
import subprocess,struct
s=Path('source/patch.c').read_text();a=s.index('static void *(*mission_intro_menu)');b=s.index('static void gangstar_mission_intro_guard(',a)
elf=Path('../work/libgl2jni.so').read_bytes()
assert struct.unpack_from('<I',elf,0xe3b350+36)[0]==0x3c3824
assert struct.unpack_from('<I',elf,0xe3b350+44)[0]==0x3c385c
assert struct.unpack_from('<I',elf,0x3c3524)[0]==0xe3a0000c
p=Path('work/tests/mission_intro.c')
p.write_text(r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
static unsigned warnings,logs,updates,draws,allocations,constructs,destroys,hides,deactivates;
#define l_warn(...) (++warnings)
#define l_info(...) (++logs)
'''+s[a:b]+r'''
static unsigned char menu[64],active,layout_memory[16];
static uintptr_t machine[6],stack[2];
static int allocation_fail,change_top,install_other;
static uintptr_t get_layout(void){uintptr_t p;memcpy(&p,menu+0x24,sizeof(p));return p;}
static void set_layout(uintptr_t p){memcpy(menu+0x24,&p,sizeof(p));}
static void *get_menu(void){return menu;}
static void *allocate(unsigned size,const char *tag,int line,int flags){
 assert(size==12 && tag && line==0 && flags==1);++allocations;return allocation_fail?0:layout_memory;
}
static void construct(void *p){
 assert(p==layout_memory);++constructs;
 // A callback from the constructor must not recursively construct another layout.
 assert(!mission_intro_restore(menu,(void*)11,machine));
 if(change_top)stack[0]=22;
 if(install_other)set_layout(123);
}
static void destroy(void *p){assert(p==layout_memory);++destroys;}
static void hide(void *hud){assert(hud==menu);++hides;}
static void deactivate(void *hud){assert(hud==menu);++deactivates;}
static void update(void *a,void *b){assert(a==(void*)11 && b==machine);assert(get_layout());++updates;}
static void draw(void *a,void *b){assert(a==(void*)11 && b==machine);assert(get_layout());++draws;}
int main(void){
 mission_intro_menu=get_menu;mission_intro_active=&active;
 mission_intro_original[0]=update;mission_intro_original[1]=draw;
 mission_intro_allocate=allocate;mission_intro_construct=construct;mission_intro_destroy=destroy;
 mission_intro_hud=get_menu;mission_intro_hide=hide;mission_intro_deactivate=deactivate;
 machine[4]=(uintptr_t)stack;machine[5]=(uintptr_t)(stack+1);stack[0]=22;
 // Missing layout for a background state: skip, never resurrect it, bound logs.
 for(unsigned i=0;i<100;++i){mission_intro_update((void*)11,machine);mission_intro_draw2d((void*)11,machine);}
 assert(!updates && !draws && !allocations && warnings==8);
 // A live top intro restores the original native layout before updating.
 stack[0]=11;mission_intro_update((void*)11,machine);
 assert(get_layout()==(uintptr_t)layout_memory && active==1 && updates==1);
 assert(allocations==1 && constructs==1 && hides==1 && deactivates==1 && logs==1);
 mission_intro_draw2d((void*)11,machine);assert(draws==1 && allocations==1);
 // Existing layouts always keep original callbacks, even if inactive/background.
 stack[0]=22;active=0;mission_intro_update((void*)11,machine);assert(updates==2);
 // Allocation failure never dispatches through a missing object.
 set_layout(0);stack[0]=11;allocation_fail=1;
 mission_intro_update((void*)11,machine);assert(updates==2 && !get_layout());allocation_fail=0;
 // A constructor that changes the state stack loses ownership of the candidate.
 change_top=1;mission_intro_update((void*)11,machine);
 assert(updates==2 && !get_layout() && destroys==1);change_top=0;
 // Respect another layout installed by a reentrant constructor callback.
 stack[0]=11;install_other=1;mission_intro_update((void*)11,machine);
 assert(updates==3 && get_layout()==123 && destroys==2);
 assert(!mission_intro_repairing && warnings==8);
}
''')
subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',str(p),'-o','work/tests/mission_intro'],check=True)
subprocess.run(['work/tests/mission_intro'],check=True)
print('Mission intro: actual native layout size/vtable verified; restore only live top state; valid dispatch, background suppression, allocation failure, reentrant state/layout changes, ownership cleanup and bounded warnings passed')
