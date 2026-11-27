#include "../source/gangstar_spawn_gc.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
void _log_print(int level,const char *format,...){(void)level;(void)format;}
static int main_thread=1;
int gangstar_wait_is_main(void){return main_thread;}

typedef struct { void **vtable; int id; } Object;
static void *table[9];
static int released[8], release_count;
static void release(void *self){
    Object *object=self;
    released[release_count++]=object->id;
    gangstar_spawn_gc_forget(object);      // what gangstar_free does for the delete
    object->vtable=(void **)0x20;           // allocator metadata overwrites the vptr
}
static int valid(const void *vtable){return vtable==table;}

int main(void){
    table[8]=(void *)release;
    Object a={table,1},b={table,2},c={table,3};
    void *storage[8];
    GangstarObjectVector queue={storage,storage,storage+8};
    gangstar_spawn_gc_queue=&queue;

    // Released in reverse order; null entries are skipped.
    storage[0]=&a;storage[1]=NULL;storage[2]=&b;queue.end=storage+3;
    assert(gangstar_spawn_gc_collect(&queue,valid)==0);
    assert(queue.end==queue.begin && release_count==2 && released[0]==2 && released[1]==1);

    // Destroyed elsewhere while queued (the crash): the free removes the entry.
    a.vtable=b.vtable=table;release_count=0;
    storage[0]=&a;storage[1]=&b;storage[2]=&c;queue.end=storage+3;
    gangstar_spawn_gc_forget(&b);
    assert(queue.end==storage+2 && storage[0]==&a && storage[1]==&c);
    b.vtable=(void **)0x20;
    assert(gangstar_spawn_gc_collect(&queue,valid)==0);
    assert(release_count==2 && released[0]==3 && released[1]==1);

    // Queued twice: the first Release frees it and drops the second copy.
    a.vtable=table;release_count=0;
    storage[0]=&a;storage[1]=&c;storage[2]=&a;queue.end=storage+3;c.vtable=table;
    assert(gangstar_spawn_gc_collect(&queue,valid)==0);
    assert(release_count==2 && released[0]==1 && released[1]==3);

    // Freed off the frame thread: queue left alone, collect skips the stale entry.
    a.vtable=c.vtable=table;release_count=0;
    storage[0]=&a;storage[1]=&c;queue.end=storage+2;
    main_thread=0;gangstar_spawn_gc_forget(&a);main_thread=1;
    assert(queue.end==storage+2);
    a.vtable=(void **)0x8de79378;
    assert(gangstar_spawn_gc_collect(&queue,valid)==1);
    assert(release_count==1 && released[0]==3 && queue.end==queue.begin);

    // Unrelated frees and an empty queue do nothing.
    storage[0]=&c;queue.end=storage+1;
    int other;gangstar_spawn_gc_forget(&other);gangstar_spawn_gc_forget(NULL);
    assert(queue.end==storage+1);
    queue.end=queue.begin;gangstar_spawn_gc_forget(&c);
    gangstar_spawn_gc_queue=NULL;gangstar_spawn_gc_forget(&c);
    puts("Spawn GC: release order, freed-while-queued, duplicates and off-thread frees passed");
    return 0;
}
