#include "gangstar_asset_perf.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
static uint64_t now = 1;
static int thread = 1, texture_calls;
static unsigned failure_records,failure_drops,api_records;
static int slow_upload;
void _log_print(int type,const char *format,...) {
    (void)type;va_list ap;va_start(ap,format);
    if(strstr(format,"RIO_UPLOAD_CALL frame=")) { ++api_records; }
    else if(strstr(format,"RIO_UPLOAD_FALLBACK frame=")) {
        ++failure_records;
        (void)va_arg(ap,unsigned);(void)va_arg(ap,unsigned);
        unsigned width=va_arg(ap,unsigned),height=va_arg(ap,unsigned);
        unsigned available=va_arg(ap,unsigned),required=va_arg(ap,unsigned),reserved=va_arg(ap,unsigned);
        assert(width==height && available<required && required<=reserved);
    } else if(strstr(format,"RIO_UPLOAD_FALLBACK dropped=")) failure_drops+=va_arg(ap,unsigned);
    va_end(ap);
}
static const char input[] = "Texture profiler must preserve decompression output and status";
uint64_t sceKernelGetProcessTimeWide(void) { now += 100; return now; }
int sceKernelGetThreadId(void) { return thread; }
void glTexImage2D(GLenum t, GLint l, GLint f, GLsizei w, GLsizei h, GLint b,
    GLenum sf, GLenum ty, const void *p) {
    assert(t == 1 && l == 2 && f == 3 && w == 4 && h == 5 && b == 0 && sf == 6 && ty == 7 && p == input);
    ++texture_calls; if(slow_upload)now+=5000;
}
void glTexSubImage2D(GLenum t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h,
    GLenum f, GLenum ty, const void *p) {
    assert(t == 1 && l == 2 && x == 3 && y == 4 && w == 5 && h == 6 && f == 7 && ty == 8 && p == input);
    ++texture_calls;
}
void glCompressedTexImage2D(GLenum t, GLint l, GLenum f, GLsizei w, GLsizei h,
    GLint b, GLsizei n, const void *p) {
    assert(t == 1 && l == 2 && f == 3 && w == 4 && h == 5 && b == 0 && n == sizeof(input) && p == input);
    ++texture_calls;
}
void glGenerateMipmap(GLenum t) { assert(t == 1); ++texture_calls; }
int main(void) {
    gangstar_asset_frame();
    gangstar_tex_image(1, 2, 3, 4, 5, 0, 6, 7, input);
    gangstar_tex_sub_image(1, 2, 3, 4, 5, 6, 7, 8, input);
    gangstar_compressed_tex_image(1, 2, 3, 4, 5, 0, sizeof(input), input);
    gangstar_generate_mipmap(1);
    unsigned char packed[256], output[256]; uLongf packed_size = sizeof(packed);
    assert(compress(packed, &packed_size, (const Bytef *)input, sizeof(input)) == Z_OK);
    uLongf output_size = sizeof(output);
    assert(gangstar_uncompress(output, &output_size, packed, packed_size) == Z_OK);
    assert(output_size == sizeof(input) && !memcmp(output, input, sizeof(input)));
    z_stream stream = {0}; assert(inflateInit(&stream) == Z_OK);
    stream.next_in = packed; stream.avail_in = packed_size;
    stream.next_out = output; stream.avail_out = sizeof(output);
    assert(gangstar_inflate(&stream, Z_FINISH) == Z_STREAM_END);
    assert(stream.total_out == sizeof(input) && !memcmp(output, input, sizeof(input)));
    assert(inflateEnd(&stream) == Z_OK);
    GangstarAssetPerf stats = gangstar_asset_stats();
    assert(texture_calls == 4 && stats.calls[RIO_ASSET_TEXTURE] == 4 && stats.us[RIO_ASSET_TEXTURE] == 400);
    assert(stats.mip_uploads == 3 && !stats.base_uploads && stats.mip_generations == 1);
    assert(stats.max_width == 5 && stats.max_height == 6);
    assert(stats.bytes[RIO_ASSET_TEXTURE] == sizeof(input));
    assert(stats.calls[RIO_ASSET_ZLIB] == 2 && stats.bytes[RIO_ASSET_ZLIB] == 2 * sizeof(input));
    for(unsigned stage=0;stage<4;++stage) {
        uint64_t began=gangstar_upload_clock();gangstar_upload_stage(stage,began,1024);
    }
    stats=gangstar_asset_stats();
    for(unsigned kind=RIO_ASSET_ETC_FREE;kind<=RIO_ASSET_ETC_SETUP;++kind)
        assert(stats.calls[kind]==1 && stats.us[kind]==100 && stats.bytes[kind]==1024);
    gangstar_upload_stage(99,1,999);
    gangstar_upload_fallback(1,1024,1024,524288,655360,699064,0,4096);
    gangstar_upload_report();assert(failure_records==1 && !failure_drops);
    for(unsigned i=0;i<65;++i) gangstar_upload_fallback(1,512,512,131072,163840,174776,0,1000);
    gangstar_upload_report();assert(failure_records==65 && failure_drops==1);
    gangstar_upload_report();assert(failure_records==65 && failure_drops==1);
    thread = 2;
    gangstar_upload_fallback(1,512,512,131072,163840,174776,0,1000);
    gangstar_upload_report();assert(failure_records==65);
    assert(!gangstar_upload_clock());gangstar_upload_stage(1,1,2048);
    gangstar_tex_image(1, 2, 3, 4, 5, 0, 6, 7, input);
    gangstar_asset_sample(RIO_ASSET_SHADER, 999, 0);
    stats = gangstar_asset_stats();
    assert(texture_calls == 5 && stats.calls[RIO_ASSET_TEXTURE] == 4 && !stats.calls[RIO_ASSET_SHADER]);
    assert(stats.calls[RIO_ASSET_ETC_ALLOC]==1 && stats.bytes[RIO_ASSET_ETC_ALLOC]==1024);
    thread = 1;
    slow_upload=1;gangstar_tex_image(1,2,3,4,5,0,6,7,input);slow_upload=0;
    gangstar_upload_report();assert(api_records==1);
    thread=2;slow_upload=1;gangstar_tex_image(1,2,3,4,5,0,6,7,input);slow_upload=0;
    gangstar_upload_report();assert(api_records==1);
    thread = 1; gangstar_asset_frame(); stats = gangstar_asset_stats();
    assert(!stats.calls[RIO_ASSET_TEXTURE] && !stats.calls[RIO_ASSET_ZLIB]);
    puts("Asset profiler: GL arguments, zlib output/status, frame reset and background exclusion passed");
}
