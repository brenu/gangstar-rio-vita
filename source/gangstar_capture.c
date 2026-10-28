/* PNG startup evidence without adding an image-library dependency. */
#include <vitaGL.h>
#include <zlib.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "utils/logger.h"
static void be32(unsigned char *p, uint32_t n) {
    p[0]=n>>24; p[1]=n>>16; p[2]=n>>8; p[3]=n;
}
static int chunk(FILE *f, const char *kind, const unsigned char *data, uint32_t n) {
    unsigned char header[8], crc[4];
    be32(header,n); memcpy(header+4,kind,4);
    uLong sum=crc32(0,(const Bytef *)kind,4);
    if (n) sum=crc32(sum,data,n);
    be32(crc,sum);
    return fwrite(header,1,8,f)==8 && (!n || fwrite(data,1,n,f)==n) && fwrite(crc,1,4,f)==4;
}
void gangstar_capture(unsigned frame) {
    const size_t row=960*3, size=(row+1)*544;
    unsigned char *pixels=malloc(row*544), *scan=malloc(size);
    uLongf compressed_size=compressBound(size);
    unsigned char *compressed=malloc(compressed_size);
    if (!pixels || !scan || !compressed) goto done;
    GLint framebuffer = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glReadPixels(0,0,960,544,GL_RGB,GL_UNSIGNED_BYTE,pixels);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    for (unsigned y=0;y<544;++y) {
        scan[y*(row+1)]=0;
        memcpy(scan+y*(row+1)+1,pixels+(543-y)*row,row);
    }
    if (compress2(compressed,&compressed_size,scan,size,1)!=Z_OK) goto done;
    char path[128]; snprintf(path,sizeof(path),DATA_PATH "frame-%u.png",frame);
    FILE *f=fopen(path,"wb");
    if (!f) goto done;
    unsigned char ihdr[13]={0}; be32(ihdr,960); be32(ihdr+4,544); ihdr[8]=8; ihdr[9]=2;
    int ok=fwrite("\x89PNG\r\n\x1a\n",1,8,f)==8 && chunk(f,"IHDR",ihdr,13) &&
        chunk(f,"IDAT",compressed,compressed_size) && chunk(f,"IEND",NULL,0);
    if (fclose(f)) ok=0;
    if (ok) l_success("Gangstar framebuffer captured: %s",path);
done:
    free(compressed); free(scan); free(pixels);
}
