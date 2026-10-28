#include "alpha_overwrite.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
 assert(vgl_alpha_complete_overwrite(1,1,1,0,0,0,1024,1024,1024,1024,1));
 assert(!vgl_alpha_complete_overwrite(0,1,1,0,0,0,1024,1024,1024,1024,1));
 assert(!vgl_alpha_complete_overwrite(1,4,1,0,0,0,1024,1024,1024,1024,1));
 assert(!vgl_alpha_complete_overwrite(1,1,2,0,0,0,1024,1024,1024,1024,1));
 assert(!vgl_alpha_complete_overwrite(1,1,1,1,0,0,1024,1024,1024,1024,1));
 assert(!vgl_alpha_complete_overwrite(1,1,1,0,1,0,1024,1024,1024,1024,1));
 assert(!vgl_alpha_complete_overwrite(1,1,1,0,0,1,1024,1024,1024,1024,1));
 assert(!vgl_alpha_complete_overwrite(1,1,1,0,0,0,512,1024,1024,1024,1));
 assert(!vgl_alpha_complete_overwrite(1,1,1,0,0,0,1024,512,1024,1024,1));
 assert(!vgl_alpha_complete_overwrite(1,1,1,0,0,0,1024,1024,1024,1024,0));
 assert(!vgl_alpha_complete_overwrite(1,1,1,0,0,0,7,8,7,8,1));
 puts("Alpha overwrite: complete image accepted; partial, mipmapped, padded and incompatible updates preserve old bytes");
}
