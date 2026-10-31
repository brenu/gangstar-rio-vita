#ifndef GANGSTAR_BUTTON_POLICY_H
#define GANGSTAR_BUTTON_POLICY_H
#include <stdint.h>
enum { RIO_CROSS=1u, RIO_SQUARE=2u, RIO_CIRCLE=4u, RIO_TRIANGLE=8u,
 RIO_L=16u, RIO_R=32u, RIO_UP=64u, RIO_DOWN=128u, RIO_LEFT=256u,
 RIO_RIGHT=512u, RIO_SELECT=1024u, RIO_START=2048u };
// Preserve UI/context buttons (shop, item, map, scripted prompts) and unknown
// actions. Only disable touch duplicates of physical gameplay bindings.
static inline int rio_touch_button_mapped(unsigned type) {
 switch(type) {
 case 0: case 2: case 9: case 10: case 11: case 13: case 16:
 case 19: case 20: case 21: case 23: case 26: return 1;
 default: return 0;
 }
}
static inline unsigned rio_triangle_action(int mode,int getup,int shop,int item) {
 return mode==1 && getup?21:mode==1 && shop?4:mode==1 && item?3:2;
}
typedef struct { unsigned held, blocked; int mode; unsigned pending_cancel; } RioPadActions;
// Consume a release only if this bridge consumed its matching press.
static inline int rio_key_consume(uint32_t *held,uint32_t mask,int active,int down) {
 if(down) { if(!active)return 0; *held|=mask; return 1; }
 if(!(*held&mask))return 0;
 *held&=~mask;return 1;
}
typedef void (*RioButtonEmit)(void *, unsigned, unsigned);
// Verified EvVirtualButton states: 0=click, 1=held, 2=release, 3=cancel.
// Styles: 0=click, 1=release action, 2=attack hold, 3=sprint hold,
// 4=context click followed by release.
static inline void rio_pad_actions(RioPadActions *p, int mode, unsigned buttons,
 RioButtonEmit emit, void *context) {
 static const struct { unsigned foot,car,type_foot,type_car,style_foot,style_car; } bindings[]={
  {RIO_CROSS,RIO_CROSS,26,23,3,1},
  // Keep attack independent of R even while the HUD changes vehicle mode.
  {RIO_SQUARE,RIO_SQUARE,0,0,2,2},
  {RIO_CIRCLE|RIO_L,RIO_CIRCLE,20,16,1,0},
  {RIO_TRIANGLE,RIO_TRIANGLE,2,2,4,1},
  {RIO_UP,RIO_UP,9,16,0,0},
  {RIO_DOWN,RIO_DOWN,10,19,1,1},
  {RIO_LEFT,RIO_LEFT,13,13,1,1},
  {RIO_RIGHT,RIO_RIGHT,11,11,1,1},
  {RIO_SELECT,RIO_SELECT,15,15,1,1}
 };
 if (mode!=p->mode) {
  for(unsigned i=0;i<sizeof(bindings)/sizeof(*bindings);++i) if(p->held&(1u<<i)) {
   unsigned style=p->mode==2?bindings[i].style_car:bindings[i].style_foot;
   if(style>=2) {
    unsigned type=p->mode==2?bindings[i].type_car:bindings[i].type_foot;
    emit(context,type,3);
    if(!mode)p->pending_cancel|=1u<<type;
   }
  }
  p->held=0;p->blocked=buttons;p->mode=mode;
 }
 p->blocked&=buttons; buttons&=~p->blocked;
 if(!mode)return;
 // The original handler may have ignored cancellation while locked. Replay
 // once it can receive input again, before accepting a fresh physical press.
 for(unsigned type=0;type<32;++type)if(p->pending_cancel&(1u<<type))emit(context,type,3);
 p->pending_cancel=0;
 for(unsigned i=0;i<sizeof(bindings)/sizeof(*bindings);++i) {
  unsigned mask=mode==2?bindings[i].car:bindings[i].foot;
  unsigned type=mode==2?bindings[i].type_car:bindings[i].type_foot;
  unsigned style=mode==2?bindings[i].style_car:bindings[i].style_foot;
  int down=(buttons&mask)!=0, was=(p->held&(1u<<i))!=0;
  if(down && !was) { if(style==0 || style==2 || style==4)emit(context,type,0);else if(style==3)emit(context,type,1); }
  else if(down && style==2)emit(context,type,1);
  else if(!down && was && style>=1)emit(context,type,2);
  if(down)p->held|=1u<<i;else p->held&=~(1u<<i);
 }
}
#endif
