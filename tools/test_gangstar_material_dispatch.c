#define _GNU_SOURCE
#include "gangstar_material_dispatch.h"
#include <sys/mman.h>
#include <assert.h>
#include <stdio.h>
static uintptr_t arena,manager,handler,node1,node2,task1,task2,function,root,props,images,values,image_reference;
static int locked,locks,unlocks,accepted,released,pending=1,names;
static uintptr_t expected_task;
static void lock_it(uintptr_t p){assert(p==manager+12 && !locked);locked=1;++locks;}
static void unlock_it(uintptr_t p){assert(p==manager+12 && locked);locked=0;++unlocks;}
static int defer_name(const char *s){assert(!strcmp(s,"middleclass_lightmap00.tga"));++names;return pending;}
static int defer_task(uintptr_t task){assert(locked);return task==task1 && rio_material_dependencies(rio_mat_ptr(task,0x30),0x7770,defer_name);}
static void accept(uintptr_t h,uintptr_t t){assert(locked && h==handler && t==expected_task);++accepted;rio_mat_put(h,16,t);}
static void release_node(uintptr_t n){assert(locked && (n==node1 || n==node2));++released;}
static const RioMaterialTaskOps ops={lock_it,unlock_it,defer_task,accept,release_node};
static void setup(void){
 memset((void *)arena,0,8192);locked=locks=unlocks=accepted=released=names=0;pending=1;
 manager=arena;handler=arena+64;node1=arena+128;node2=arena+160;task1=arena+192;task2=arena+256;
 function=arena+320;uintptr_t db=arena+384,res=arena+448,header=arena+512;root=arena+576;
 uintptr_t materials=arena+768;props=arena+896;images=arena+1024;values=arena+1088;image_reference=arena+1120;
 rio_mat_put(manager,0,node1);rio_mat_put(manager,4,node2);*(unsigned char *)(manager+28)=1;
 rio_mat_put(node1,0,node2);rio_mat_put(node1,4,manager);rio_mat_put(node1,8,task1);
 rio_mat_put(node2,0,manager);rio_mat_put(node2,4,node1);rio_mat_put(node2,8,task2);
 rio_mat_put(handler,4,1);rio_mat_put(task1,4,1);rio_mat_put(task2,4,1);rio_mat_put(task1,0x30,function);
 rio_mat_put(function,0,0x7770);rio_mat_put(function,4,db);rio_mat_put(db,0,res);rio_mat_put(res,16,header);rio_mat_put(header,32,root);
 rio_mat_put(root,0x54,1);rio_mat_put(root,0x58,images);rio_mat_put(root,0x64,1);rio_mat_put(root,0x68,materials);
 rio_mat_put(materials,16,1);rio_mat_put(materials,20,props);rio_mat_put(props,8,12);rio_mat_put(props,20,values);rio_mat_put(values,0,image_reference);rio_mat_put(image_reference,0,images);
 rio_mat_put(images,8,arena+1152);strcpy((char *)(arena+1152),"middleclass_lightmap00.tga");
}
int main(void){
 void *memory=mmap(NULL,8192,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
 assert(memory!=MAP_FAILED && (uintptr_t)memory+8192<=UINT32_MAX);arena=(uintptr_t)memory;
 setup();rio_mat_put(props,8,13);expected_task=task2;assert(rio_material_dispatch(manager,handler,&ops));
 assert(accepted==1 && released==1 && locks==1 && unlocks==1 && names==1);
 assert(rio_mat_ptr(manager,0)==node1 && rio_mat_ptr(manager,4)==node1 && rio_mat_ptr(node1,0)==manager && rio_mat_ptr(node1,4)==manager);
 assert(!*(unsigned char *)(task1+0x2d)); // deferred task not completed, freed, or accepted
 assert(!rio_material_dispatch(manager,handler,&ops) && accepted==1 && released==1);
 pending=0;expected_task=task1;assert(rio_material_dispatch(manager,handler,&ops));assert(rio_mat_ptr(manager,0)==manager && rio_mat_ptr(manager,4)==manager);
 setup();*(unsigned char *)(manager+28)=0;assert(!rio_material_dispatch(manager,handler,&ops) && !locks);
 setup();rio_mat_put(task1,4,2);expected_task=task2;assert(rio_material_dispatch(manager,handler,&ops) && !names);
 setup();rio_mat_put(image_reference,0,images+1);assert(!rio_material_dependencies(function,0x7770,defer_name) && !names);
 rio_mat_put(image_reference,0,images+16);assert(!rio_material_dependencies(function,0x7770,defer_name));
 setup();assert(rio_material_dependencies(function,0x7770,defer_name) && names==1);
 setup();rio_mat_put(props,8,13);assert(rio_material_dependencies(function,0x7770,defer_name) && names==1);
 setup();rio_mat_put(props,8,11);assert(!rio_material_dependencies(function,0x7770,defer_name));
 setup();rio_mat_put(root,0x54,9000);assert(!rio_material_dependencies(function,0x7770,defer_name));
 setup();rio_mat_put(root,0x64,0);assert(!rio_material_dependencies(function,0x7770,defer_name));
 setup();rio_mat_put(function,0,0x8880);assert(!rio_material_dependencies(function,0x7770,defer_name));
 assert(munmap(memory,8192)==0);puts("Material dispatch: deferred ownership/dependencies, ready-task bypass, native unlink/free order, masks and image-layout guards passed");
}
