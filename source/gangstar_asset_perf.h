#ifndef GANGSTAR_ASSET_PERF_H
#define GANGSTAR_ASSET_PERF_H
#include <stdint.h>
#include <stddef.h>
#include <vitaGL.h>
#include <zlib.h>
enum { RIO_ASSET_TEXTURE, RIO_ASSET_ZLIB, RIO_ASSET_SHADER, RIO_ASSET_ETC_FREE, RIO_ASSET_ETC_ALLOC, RIO_ASSET_ETC_SWIZZLE, RIO_ASSET_ETC_SETUP, RIO_ASSET_ETC_RESERVE, RIO_ASSET_ETC_REUSE, RIO_ASSET_ETC_FALLBACK, RIO_ASSET_ETC_MIGRATED, RIO_ASSET_ETC_NO_ROOM, RIO_ASSET_KINDS };
typedef struct { uint64_t us[RIO_ASSET_KINDS], bytes[RIO_ASSET_KINDS]; unsigned calls[RIO_ASSET_KINDS]; unsigned base_uploads, mip_uploads, mip_generations, max_width, max_height; } GangstarAssetPerf;
void gangstar_asset_frame(void);
void gangstar_upload_fallback(unsigned,unsigned,unsigned,size_t,size_t,size_t,unsigned,size_t);
void gangstar_upload_report(void);
uint64_t gangstar_upload_clock(void);
void gangstar_upload_stage(unsigned,uint64_t,uint64_t);
GangstarAssetPerf gangstar_asset_stats(void);
void gangstar_asset_sample(unsigned kind, uint64_t us, uint64_t bytes);
void gangstar_tex_image(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *);
void gangstar_tex_sub_image(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *);
void gangstar_compressed_tex_image(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *);
void gangstar_generate_mipmap(GLenum);
int gangstar_inflate(z_streamp, int);
int gangstar_uncompress(Bytef *, uLongf *, const Bytef *, uLong);
#endif
