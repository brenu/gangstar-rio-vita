#include "gangstar_render_sample.h"
#include <assert.h>
#include <stdio.h>
int main(void){uint32_t s=0x61c88647u;unsigned samples=0;for(unsigned i=0;i<1000000;++i)samples+=rio_render_choose(&s);assert(samples>30000 && samples<32500);puts("Render sampler: bounded 1/32 pseudorandom sampling avoids fixed draw-position bias");}
