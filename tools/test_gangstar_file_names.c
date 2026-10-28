#include "gangstar_file_names.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
 char path[240],out[192];memset(path,'x',239);path[239]=0;
 rio_file_name_set(1,7,path);memset(path,'y',239);
 rio_file_name_get(1,7,out);assert(strlen(out)==191 && out[0]=='x');
 rio_file_name_set(2,7,"fd");rio_file_name_get(2,7,out);assert(!strcmp(out,"fd"));
 rio_file_name_clear(1,7);rio_file_name_get(1,7,out);assert(!strcmp(out,"<untracked>"));
 rio_file_name_get(2,7,out);assert(!strcmp(out,"fd"));
 rio_file_name_clear(2,7);
 for(unsigned i=0;i<128;++i)rio_file_name_set(1,i,"original");
 rio_file_name_set(1,200,"overflow");rio_file_name_get(1,200,out);assert(!strcmp(out,"<untracked>"));
 rio_file_name_clear(1,12);rio_file_name_set(1,12,"reused");
 rio_file_name_get(1,12,out);assert(!strcmp(out,"reused"));
 puts("File provenance: owned bounded names, FILE/fd namespace isolation, close/reuse and saturation fallback passed");
}
