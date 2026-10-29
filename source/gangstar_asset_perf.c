#include "gangstar_asset_perf.h"
#include "utils/logger.h"
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <stdatomic.h>
static atomic_int frame_thread;
static GangstarAssetPerf stats;
static unsigned frame_number=(unsigned)-1;
void gangstar_upload_layout_error(int level,unsigned base_w,unsigned base_h,unsigned w,unsigned h) {
    static atomic_uint reported;
    if (atomic_fetch_add(&reported,1) < 16)
        l_error("RIO_UPLOAD_LAYOUT_REJECT frame=%u level=%d base=%ux%u incoming=%ux%u",
            frame_number,level,base_w,base_h,w,h);
}
static struct { unsigned frame,level,width,height,pool; size_t available,required,reserved,pool_free; } upload_failures[64];
static unsigned upload_failure_count, upload_failure_dropped;
static unsigned alpha_discard_calls;
static uint64_t alpha_discard_bytes;
void gangstar_alpha_discard(size_t bytes) {
    if (atomic_load_explicit(&frame_thread,memory_order_relaxed)!=sceKernelGetThreadId()) return;
    ++alpha_discard_calls; alpha_discard_bytes+=bytes;
}
#ifdef GANGSTAR_UPLOAD_STAGES
static struct { unsigned frame,api; GLint level,format; GLenum type,source_format; GLsizei width,height;
    unsigned null_data; uintptr_t caller; uint64_t us; } upload_calls[64];
static unsigned upload_call_count,upload_call_dropped;
static void upload_call(unsigned api,uint64_t began,uint64_t elapsed,GLint level,GLint format,
    GLenum type,GLenum source_format,GLsizei width,GLsizei height,const void *data,uintptr_t caller) {
    if(!began || elapsed<2000)return;
    if(upload_call_count==64){++upload_call_dropped;return;}
    upload_calls[upload_call_count++]=(__typeof__(upload_calls[0])){
        frame_number,api,level,format,type,source_format,width,height,data==NULL,caller,elapsed};
}
#else
#define upload_call(...) ((void)0)
#endif
void gangstar_asset_frame(void) {
    ++frame_number;
    atomic_store_explicit(&frame_thread, sceKernelGetThreadId(), memory_order_relaxed);
    stats = (GangstarAssetPerf){0};
}
GangstarAssetPerf gangstar_asset_stats(void) { return stats; }
static uint64_t start(void) {
    return atomic_load_explicit(&frame_thread, memory_order_relaxed) == sceKernelGetThreadId()
        ? sceKernelGetProcessTimeWide() : 0;
}
void gangstar_asset_sample(unsigned kind, uint64_t us, uint64_t bytes) {
    if (kind >= RIO_ASSET_KINDS ||
        atomic_load_explicit(&frame_thread, memory_order_relaxed) != sceKernelGetThreadId()) return;
    ++stats.calls[kind]; stats.us[kind] += us; stats.bytes[kind] += bytes;
}
static uint64_t finish(unsigned kind, uint64_t began, uint64_t bytes) {
    if(!began)return 0;
    uint64_t elapsed=sceKernelGetProcessTimeWide()-began;
    gangstar_asset_sample(kind,elapsed,bytes);return elapsed;
}
static void texture_level(uint64_t began, GLint level, GLsizei width, GLsizei height) {
    if (!began) return;
    if (level == 0) ++stats.base_uploads;
    else if (level > 0) ++stats.mip_uploads;
    if (width > 0 && (unsigned)width > stats.max_width) stats.max_width = width;
    if (height > 0 && (unsigned)height > stats.max_height) stats.max_height = height;
}
void gangstar_generate_mipmap(GLenum target) {
    uint64_t began = start();
    glGenerateMipmap(target);
    if (began) ++stats.mip_generations;
    uint64_t elapsed=finish(RIO_ASSET_TEXTURE,began,0);(void)elapsed;
    upload_call(2,began,elapsed,0,0,0,0,0,0,NULL,(uintptr_t)__builtin_return_address(0));
}
void gangstar_tex_image(GLenum target, GLint level, GLint format, GLsizei width,
    GLsizei height, GLint border, GLenum source_format, GLenum type, const void *data) {
    uint64_t began = start();
    glTexImage2D(target, level, format, width, height, border, source_format, type, data);
    texture_level(began, level, width, height);
    uint64_t elapsed=finish(RIO_ASSET_TEXTURE,began,0);(void)elapsed;
    upload_call(0,began,elapsed,level,format,type,source_format,width,height,data,(uintptr_t)__builtin_return_address(0));
}
void gangstar_tex_sub_image(GLenum target, GLint level, GLint x, GLint y, GLsizei width,
    GLsizei height, GLenum format, GLenum type, const void *data) {
    uint64_t began = start();
    glTexSubImage2D(target, level, x, y, width, height, format, type, data);
    texture_level(began, level, width, height);
    uint64_t elapsed=finish(RIO_ASSET_TEXTURE,began,0);(void)elapsed;
    upload_call(1,began,elapsed,level,format,type,format,width,height,data,(uintptr_t)__builtin_return_address(0));
}
void gangstar_compressed_tex_image(GLenum target, GLint level, GLenum format, GLsizei width,
    GLsizei height, GLint border, GLsizei size, const void *data) {
    uint64_t began = start();
    glCompressedTexImage2D(target, level, format, width, height, border, size, data);
    texture_level(began, level, width, height);
    uint64_t elapsed=finish(RIO_ASSET_TEXTURE,began,size>0?(uint64_t)size:0);(void)elapsed;
    if(format!=0x8d64) {
        upload_call(3,began,elapsed,level,format,0,format,width,height,data,(uintptr_t)__builtin_return_address(0));
    }
}
int gangstar_inflate(z_streamp stream, int flush) {
    uint64_t began = start();
    uInt before = stream ? stream->avail_out : 0;
    int result = inflate(stream, flush);
    uint64_t bytes = stream && stream->avail_out <= before ? before - stream->avail_out : 0;
    finish(RIO_ASSET_ZLIB, began, bytes);
    return result;
}
int gangstar_uncompress(Bytef *destination, uLongf *length, const Bytef *source, uLong size) {
    uint64_t began = start();
    int result = uncompress(destination, length, source, size);
    finish(RIO_ASSET_ZLIB, began, result == Z_OK && length ? *length : 0);
    return result;
}

// Optional vitaGL internal stage callbacks. No logging/allocations here.
uint64_t gangstar_upload_clock(void) { return start(); }
void gangstar_upload_stage(unsigned kind,uint64_t began,uint64_t bytes) {
    if (!began) return;
    static const unsigned kinds[]={RIO_ASSET_ETC_FREE,RIO_ASSET_ETC_ALLOC,RIO_ASSET_ETC_SWIZZLE,RIO_ASSET_ETC_SETUP,RIO_ASSET_ETC_RESERVE,RIO_ASSET_ETC_REUSE,RIO_ASSET_ETC_FALLBACK,RIO_ASSET_ETC_MIGRATED,RIO_ASSET_ETC_NO_ROOM};
    if (kind<sizeof(kinds)/sizeof(*kinds)) finish(kinds[kind],began,bytes);
}

void gangstar_upload_fallback(unsigned level,unsigned width,unsigned height,size_t available,
    size_t required,size_t reserved,unsigned pool,size_t pool_free) {
    if (atomic_load_explicit(&frame_thread,memory_order_relaxed)!=sceKernelGetThreadId()) return;
    if (upload_failure_count==64) { ++upload_failure_dropped; return; }
    upload_failures[upload_failure_count++]=(__typeof__(upload_failures[0])){
        frame_number,level,width,height,pool,available,required,reserved,pool_free};
}
void gangstar_upload_report(void) {
    if(alpha_discard_calls) { l_info("RIO_ALPHA_OVERWRITE calls=%u avoided_copy_bytes=%llu",alpha_discard_calls,(unsigned long long)alpha_discard_bytes); }
    alpha_discard_calls=0; alpha_discard_bytes=0;
#ifdef GANGSTAR_UPLOAD_STAGES
    static const char *const apis[]={"tex_image","tex_sub_image","generate_mipmap","compressed_image"};
    for(unsigned i=0;i<upload_call_count;++i){
        __typeof__(upload_calls[0]) e=upload_calls[i];(void)e;(void)apis;
        l_info("RIO_UPLOAD_CALL frame=%u api=%s elapsed_us=%llu level=%d format=0x%x source_format=0x%x type=0x%x width=%d height=%d null_data=%u caller=0x%x",
            e.frame,apis[e.api],(unsigned long long)e.us,e.level,e.format,e.source_format,e.type,e.width,e.height,e.null_data,(unsigned)e.caller);
    }
    if(upload_call_dropped){l_info("RIO_UPLOAD_CALL dropped=%u",upload_call_dropped);}
    upload_call_count=upload_call_dropped=0;
#endif
    for (unsigned i=0;i<upload_failure_count;++i) {
        __typeof__(upload_failures[0]) e=upload_failures[i]; (void)e;
        l_info("RIO_UPLOAD_FALLBACK frame=%u level=%u width=%u height=%u available=%u required=%u reserved=%u pool=%u pool_free=%u",
            e.frame,e.level,e.width,e.height,(unsigned)e.available,(unsigned)e.required,
            (unsigned)e.reserved,e.pool,(unsigned)e.pool_free);
    }
    if (upload_failure_dropped) { l_info("RIO_UPLOAD_FALLBACK dropped=%u",upload_failure_dropped); }
    upload_failure_count=upload_failure_dropped=0;
}
