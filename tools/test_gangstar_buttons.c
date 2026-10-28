#include "gangstar_button_policy.h"
#include <assert.h>
#include <stdio.h>
static unsigned count,types[128],states[128];
static void emit(void *unused,unsigned type,unsigned state){(void)unused;assert(count<128);types[count]=type;states[count++]=state;}
static void reset(void){count=0;}
int main(void){
 assert(rio_triangle_action(1,1,1)==4 && rio_triangle_action(1,0,1)==3);
 assert(rio_triangle_action(1,0,0)==2 && rio_triangle_action(2,1,1)==2);
 RioPadActions p={0};rio_pad_actions(&p,1,0,emit,0);
 rio_pad_actions(&p,1,RIO_CROSS,emit,0);assert(count==1&&types[0]==26&&states[0]==1);
 rio_pad_actions(&p,1,RIO_CROSS,emit,0);assert(count==1);
 rio_pad_actions(&p,1,0,emit,0);assert(count==2&&states[1]==2);
 reset();rio_pad_actions(&p,1,RIO_R|RIO_SQUARE,emit,0);assert(count==1&&types[0]==0&&states[0]==0);
 rio_pad_actions(&p,1,RIO_SQUARE,emit,0);assert(count==2&&states[1]==1);
 rio_pad_actions(&p,0,RIO_SQUARE,emit,0);assert(count==3&&states[2]==3);
 reset();rio_pad_actions(&p,1,RIO_SQUARE,emit,0);assert(!count);
 rio_pad_actions(&p,1,0,emit,0);assert(!count);
 rio_pad_actions(&p,1,RIO_TRIANGLE,emit,0);assert(count==1&&types[0]==2&&states[0]==0);
 rio_pad_actions(&p,1,0,emit,0);assert(count==2&&states[1]==2);
 reset();rio_pad_actions(&p,1,RIO_LEFT|RIO_RIGHT|RIO_SELECT|RIO_CIRCLE,emit,0);assert(!count);
 rio_pad_actions(&p,1,0,emit,0);assert(count==4);
 for(unsigned i=0;i<count;++i)assert(states[i]==2);
 reset();rio_pad_actions(&p,2,0,emit,0);rio_pad_actions(&p,2,RIO_CROSS,emit,0);assert(!count);
 rio_pad_actions(&p,2,0,emit,0);assert(count==1&&types[0]==23&&states[0]==2);
 reset();rio_pad_actions(&p,2,RIO_L|RIO_R,emit,0);assert(!count);
 rio_pad_actions(&p,2,0,emit,0);assert(!count);
 rio_pad_actions(&p,2,RIO_CIRCLE,emit,0);assert(count==1&&types[0]==16&&states[0]==0);
 reset();rio_pad_actions(&p,2,0,emit,0);rio_pad_actions(&p,2,RIO_TRIANGLE,emit,0);assert(!count);
 rio_pad_actions(&p,2,0,emit,0);assert(count==1&&types[0]==2&&states[0]==2);
 reset();rio_pad_actions(&p,2,RIO_START,emit,0);rio_pad_actions(&p,2,0,emit,0);assert(!count);
 puts("Button actions: native states, sprint, attack hold/combined sources, context cancellation, held-input blocking, enter car, weapon/map/cover release, nitro and pedal isolation passed");
}
