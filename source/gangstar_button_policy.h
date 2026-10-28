#ifndef GANGSTAR_BUTTON_POLICY_H
#define GANGSTAR_BUTTON_POLICY_H
#include <stdint.h>
enum { RIO_CROSS=1u, RIO_SQUARE=2u, RIO_CIRCLE=4u, RIO_TRIANGLE=8u,
 RIO_L=16u, RIO_R=32u, RIO_UP=64u, RIO_DOWN=128u, RIO_LEFT=256u,
 RIO_RIGHT=512u, RIO_SELECT=1024u, RIO_START=2048u };
static inline unsigned rio_triangle_action(int mode,int shop,int item) {
 return mode==1 && shop?4:mode==1 && item?3:2;
}
typedef struct { unsigned held, blocked; int mode; } RioPadActions;
typedef void (*RioButtonEmit)(void *, unsigned, unsigned);
// Verified EvVirtualButton states: 0=click, 1=held, 2=release, 3=cancel.
// Styles: 0=click, 1=release action, 2=attack hold, 3=sprint hold,
// 4=context click followed by release.
static inline void rio_pad_actions(RioPadActions *p, int mode, unsigned buttons,
 RioButtonEmit emit, void *context) {
 static const struct { unsigned foot,car,type_foot,type_car,style_foot,style_car; } bindings[]={
  {RIO_CROSS,RIO_CROSS,26,23,3,1},
  {RIO_SQUARE|RIO_R,RIO_SQUARE,0,0,2,2},
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
   if(style>=2) emit(context,p->mode==2?bindings[i].type_car:bindings[i].type_foot,3);
  }
  p->held=0;p->blocked=buttons;p->mode=mode;
 }
 p->blocked&=buttons; buttons&=~p->blocked;
 if(!mode)return;
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
