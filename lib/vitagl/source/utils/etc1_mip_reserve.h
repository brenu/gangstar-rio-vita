#ifndef VGL_ETC1_MIP_RESERVE_H
#define VGL_ETC1_MIP_RESERVE_H
#include <stddef.h>
// Start reservation only once a second mip is requested. Base-only textures
// keep their original allocation. Visible mip count is never advanced here.
static inline int vgl_etc1_reserve_layout(int native_etc,int level,unsigned w,unsigned h) {
    if(!native_etc || !w || !h || w>4096 || h>4096 || (w&(w-1)) || (h&(h-1))) return 0;
    unsigned maximum=w>h?w:h,last=0;
    for(unsigned edge=maximum;edge>1;edge>>=1)++last;
    return level>=1 && (unsigned)level<=last;
}
static inline size_t vgl_etc1_mip_reserve(int native_etc,int level,unsigned w,unsigned h,size_t required) {
    if (!vgl_etc1_reserve_layout(native_etc,level,w,h)) return required;
    size_t bytes=0;
    for(unsigned width=w,height=h;;) {
        bytes+=(size_t)((width+3)/4)*((height+3)/4)*8;
        if(width==1 && height==1) break;
        if(width>1) width>>=1;
        if(height>1) height>>=1;
    }
    return required<=bytes ? bytes : required;
}
#endif
