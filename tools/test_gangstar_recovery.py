"""Exercise actual Triangle dispatch with hidden recovery UI and mode changes."""
from pathlib import Path
import subprocess,struct
s=Path('source/gangstar_controls.c').read_text();a=s.index('static uint32_t current_swap_item(');b=s.index('int gangstar_controls_filter_key(',a)
# Verify native event type25, Item payload and near-weapon lifetime field.
elf=Path('../work/libgl2jni.so').read_bytes()
pc=0x441988+25*4;word=struct.unpack_from('<I',elf,pc)[0]
imm=word&0xffffff
if imm&0x800000:imm-=0x1000000
assert pc+8+imm*4==0x441a24
assert struct.unpack_from('<I',elf,0x4420dc)[0]==0xe5940014
assert struct.unpack_from('<I',elf,0x44cfc4)[0]==0xe5832b94
p=Path('work/tests/recovery.c')
p.write_text(r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "gangstar_button_policy.h"
static unsigned char player[4096];
static void *player_pointer=player;
static void **player_instance=&player_pointer;
static int last_mode=1,visible,shop,item;
static uint32_t triangle_weapon;
static unsigned triangle_action=2,virtual_vtable=123,last_button,last_state;
static void *hud_instance(void){return player;}
static int getup_active(void *p){(void)p;return visible;}
static int near_shop(void *p){(void)p;return shop;}
static int near_item(void *p){(void)p;return item;}
static int player_fallen(void *p){return ((unsigned char*)p)[0x77c];}
static void *event_instance(void){return player;}
static void raise_event(void *target,const void *event){
 (void)target;const uint32_t *e=event;assert(e[0]==123 && e[1]==13);
 last_state=e[2];last_button=e[3];
 if(last_button==25)assert(e[4]==UINT32_MAX && e[5]==0x12345678);
 if(last_button==21 && last_state==2){unsigned n;memcpy(&n,player+0x908,4);n+=500;memcpy(player+0x908,&n,4);}
}
#define l_info(...) ((void)0)
'''+s[a:b]+r'''
int main(void){
 send_button(0,2,0);assert(last_button==2);send_button(0,2,2);assert(last_button==2);
 player[0x77c]=1;shop=item=1;
 send_button(0,2,0);assert(last_button==21 && last_state==0);
 player[0x77c]=0; // Release still goes to the action selected on press.
 send_button(0,2,2);assert(last_button==21 && last_state==2);
 unsigned count;memcpy(&count,player+0x908,4);assert(count==500);
 // Dedicated bridge works with ordinary controls inactive; all other buttons are masked.
 last_mode=0;RioPadActions recovery={0};
 rio_pad_actions(&recovery,1,0,send_recovery_button,0);
 rio_pad_actions(&recovery,1,(RIO_TRIANGLE|RIO_SQUARE|RIO_R)&RIO_TRIANGLE,send_recovery_button,0);
 assert(last_button==21 && last_state==0);
 rio_pad_actions(&recovery,1,0,send_recovery_button,0);
 assert(last_button==21 && last_state==2);
 memcpy(&count,player+0x908,4);assert(count==1000);

 // Ground-weapon swaps carry the exact native item pointer on release.
 uint32_t pickup=0x12345678;memcpy(player+0xb94,&pickup,4);last_mode=1;shop=item=0;
 send_button(0,2,0);assert(last_button==25 && last_state==0);
 send_button(0,2,2);assert(last_button==25 && last_state==2);
 pickup=0;memcpy(player+0xb94,&pickup,4);last_button=99;
 send_button(0,2,2);assert(last_button==99); // target disappeared after press
 triangle_action=21;last_mode=2;send_button(0,2,3);assert(last_button==21 && last_state==3);
 last_mode=1;player[0x77c]=1;player[0x77d]=1;shop=item=0;
 send_button(0,2,0);assert(last_button==2);
 player[0x77d]=0;unsigned vehicle=1;memcpy(player+0x778,&vehicle,4);
 send_button(0,2,0);assert(last_button==2);
}
''')
subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-Isource',str(p),'-o','work/tests/recovery'],check=True)
subprocess.run(['work/tests/recovery'],check=True)
print('Triangle dispatch: hidden UI recovery, recovery priority, matching release after state change, mode-change cancellation, blocked/vehicle exclusion, ground-weapon item payload and disappeared-target cancellation passed')
