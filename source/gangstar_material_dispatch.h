#ifndef GANGSTAR_MATERIAL_DISPATCH_H
#define GANGSTAR_MATERIAL_DISPATCH_H
#include <stdint.h>
#include <string.h>
// Native ABI is 32-bit even in the host-side layout tests.
static inline uintptr_t rio_mat_ptr(uintptr_t p,unsigned offset){uint32_t v;memcpy(&v,(void *)(p+offset),4);return v;}
static inline void rio_mat_put(uintptr_t p,unsigned offset,uintptr_t v){uint32_t n=(uint32_t)v;memcpy((void *)(p+offset),&n,4);}
typedef struct {void (*lock)(uintptr_t);void (*unlock)(uintptr_t);int (*defer)(uintptr_t);void (*accept)(uintptr_t,uintptr_t);void (*release)(uintptr_t);} RioMaterialTaskOps;
// Mirrors only the nonblocking native dispatch operation, preserving ownership
// and all dependency/completion processing in the original task runner.
static inline int rio_material_dispatch(uintptr_t manager,uintptr_t handler,const RioMaterialTaskOps *ops) {
 if(!*(unsigned char *)(manager+0x1c))return 0;
 ops->lock(manager+0xc);
 uint32_t mask=(uint32_t)rio_mat_ptr(handler,4);unsigned scanned=0;int result=0;
 for(uintptr_t node=rio_mat_ptr(manager,0);node!=manager && scanned++<4096;node=rio_mat_ptr(node,0)) {
  uintptr_t task=rio_mat_ptr(node,8);
  if(!task || !(mask&(uint32_t)rio_mat_ptr(task,4)) || ops->defer(task))continue;
  ops->accept(handler,task);
  uintptr_t next=rio_mat_ptr(node,0),previous=rio_mat_ptr(node,4);
  rio_mat_put(previous,0,next);rio_mat_put(next,4,previous);
  ops->release(node);result=1;break;
 }
 ops->unlock(manager+0xc);return result;
}
// A material task owns its database while queued. Only image pointers belonging
// to that database's declared image table are inspected. Native image-reference
// types 12 and 13 both call constructImage with the same SImage** layout.
static inline int rio_material_dependencies(uintptr_t function,uintptr_t expected_vtable,int (*defer)(const char *)) {
 if(rio_mat_ptr(function,0)!=expected_vtable)return 0;
 uintptr_t db=rio_mat_ptr(function,4);if(!db)return 0;
 uintptr_t resource=rio_mat_ptr(db,0);if(!resource)return 0;
 uintptr_t header=rio_mat_ptr(resource,0x10);if(!header)return 0;
 uintptr_t root=rio_mat_ptr(header,0x20);if(!root)return 0;
 unsigned images=(unsigned)rio_mat_ptr(root,0x54),materials=(unsigned)rio_mat_ptr(root,0x64);
 uintptr_t image_table=rio_mat_ptr(root,0x58),material_table=rio_mat_ptr(root,0x68);
 uint16_t id;memcpy(&id,(void *)(function+8),2);
 if(!images || images>8192 || !image_table || materials>8192 || id>=materials || !material_table)return 0;
 uintptr_t material=material_table+(uintptr_t)id*36;
 unsigned count=(unsigned)rio_mat_ptr(material,16);uintptr_t properties=rio_mat_ptr(material,20);
 if(count>128 || (count && !properties))return 0;
 for(unsigned i=0;i<count;++i) {
  uintptr_t value=properties+(uintptr_t)i*24;
  unsigned type=(unsigned)rio_mat_ptr(value,8);
  if(type!=12 && type!=13)continue;
  uintptr_t values=rio_mat_ptr(value,20);if(!values)continue;
  // Native values contain SImage** references, not direct SImage* values.
  uintptr_t reference=rio_mat_ptr(values,0);if(!reference || reference%4)continue;
  uintptr_t image=rio_mat_ptr(reference,0);
  if(image<image_table || image-image_table>=(uintptr_t)images*16 || (image-image_table)%16)continue;
  const char *name=(const char *)rio_mat_ptr(image,8);
  if(name && defer(name))return 1;
 }
 return 0;
}
#endif
