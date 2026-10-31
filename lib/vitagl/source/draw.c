/*
 * This file is part of vitaGL
 * Copyright 2017, 2018, 2019, 2020 Rinnegatamante
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* 
 * draw.c:
 * Implementation for draw call functions
 */
#include "shared.h"
#include "vitaGL.h"

#ifdef RIO_VERTEX_RANGE_COPY
#include "utils/vertex_range.h"
static unsigned rio_range_draws, rio_range_trimmed;
static uint64_t rio_range_saved;
void vglGetVertexRangeStats(unsigned *draws,unsigned *trimmed,uint64_t *saved) {
 THREAD_SAFE()
 *draws=rio_range_draws;*trimmed=rio_range_trimmed;*saved=rio_range_saved;
 rio_range_draws=rio_range_trimmed=0;rio_range_saved=0;
}
#endif
// Optional loader-side sampling. Ordinary vitaGL builds have no profiler.
extern uint64_t gangstar_render_stage_clock(void) __attribute__((weak));
extern void gangstar_render_stage_end(unsigned,uint64_t) __attribute__((weak));
#define RIO_STAGE_BEGIN() (gangstar_render_stage_clock?gangstar_render_stage_clock():0)
#define RIO_STAGE_END(stage,began) do {if((began) && gangstar_render_stage_end)gangstar_render_stage_end(stage,began);} while(0)

GLboolean prim_is_non_native = GL_FALSE; // Flag for when a primitive not supported natively by sceGxm is used

#ifdef RIO_SELECTIVE_VERTEX_COPY
static unsigned rio_rejected_draw_setups;
void vglGetRejectedDrawSetupStats(unsigned *rejected) {
 THREAD_SAFE()
 *rejected=rio_rejected_draw_setups;rio_rejected_draw_setups=0;
}
#endif
// Vertex setup may fail for allocation/bounds reasons even with API error
// checks disabled. Never submit a draw using streams left by an earlier draw.
static inline GLboolean rio_draw_setup_ready(GLboolean ready) {
#ifdef RIO_SELECTIVE_VERTEX_COPY
 if(!ready)++rio_rejected_draw_setups;
#endif
 return ready;
}


#ifndef INDICES_DRAW_SPEEDHACK
#define setup_elements_indices(type_t) \
	type_t *ptr; \
	if (gpu_buf != NULL && !prim_is_non_native) { \
		ptr = src; \
		gpu_buf->last_frame = vgl_framecount; \
	} else { \
		switch (mode) { \
		case GL_QUADS: \
			ptr = gpu_alloc_mapped_temp(count * 3 * sizeof(type_t)); \
			for (GLsizei i = 0; i < count / 4; i++) { \
				ptr[i * 6] = src[i * 4]; \
				ptr[i * 6 + 1] = src[i * 4 + 1]; \
				ptr[i * 6 + 2] = src[i * 4 + 3]; \
				ptr[i * 6 + 3] = src[i * 4 + 1]; \
				ptr[i * 6 + 4] = src[i * 4 + 2]; \
				ptr[i * 6 + 5] = src[i * 4 + 3]; \
			} \
			count = (count / 2) * 3; \
			break; \
		case GL_LINE_STRIP: \
			ptr = gpu_alloc_mapped_temp((count - 1) * 2 * sizeof(type_t)); \
			for (GLsizei i = 0; i < count - 1; i++) { \
				ptr[i * 2] = src[i]; \
				ptr[i * 2 + 1] = src[i + 1]; \
			} \
			count = (count - 1) * 2; \
			break; \
		case GL_LINE_LOOP: \
			ptr = gpu_alloc_mapped_temp(count * 2 * sizeof(type_t)); \
			for (GLsizei i = 0; i < count - 1; i++) { \
				ptr[i * 2] = src[i]; \
				ptr[i * 2 + 1] = src[i + 1]; \
			} \
			ptr[(count - 1) * 2] = src[count - 1]; \
			ptr[(count - 1) * 2 + 1] = src[0]; \
			count = count * 2; \
			break; \
		default: \
			ptr = gpu_alloc_mapped_temp(count * sizeof(type_t)); \
			vgl_fast_memcpy(ptr, src, count * sizeof(type_t)); \
			break; \
		} \
	}
#else
#define setup_elements_indices(type_t) \
	type_t *ptr; \
	if (gpu_buf != NULL && !prim_is_non_native) { \
		ptr = src; \
		gpu_buf->last_frame = vgl_framecount; \
	} else { \
		switch (mode) { \
		case GL_QUADS: \
			ptr = gpu_alloc_mapped_temp(count * 3 * sizeof(type_t)); \
			for (GLsizei i = 0; i < count / 4; i++) { \
				ptr[i * 6] = src[i * 4]; \
				ptr[i * 6 + 1] = src[i * 4 + 1]; \
				ptr[i * 6 + 2] = src[i * 4 + 3]; \
				ptr[i * 6 + 3] = src[i * 4 + 1]; \
				ptr[i * 6 + 4] = src[i * 4 + 2]; \
				ptr[i * 6 + 5] = src[i * 4 + 3]; \
			} \
			count = (count / 2) * 3; \
			break; \
		case GL_LINE_STRIP: \
			ptr = gpu_alloc_mapped_temp((count - 1) * 2 * sizeof(type_t)); \
			for (GLsizei i = 0; i < count - 1; i++) { \
				ptr[i * 2] = src[i]; \
				ptr[i * 2 + 1] = src[i + 1]; \
			} \
			count = (count - 1) * 2; \
			break; \
		case GL_LINE_LOOP: \
			ptr = gpu_alloc_mapped_temp(count * 2 * sizeof(type_t)); \
			for (GLsizei i = 0; i < count - 1; i++) { \
				ptr[i * 2] = src[i]; \
				ptr[i * 2 + 1] = src[i + 1]; \
			} \
			ptr[(count - 1) * 2] = src[count - 1]; \
			ptr[(count - 1) * 2 + 1] = src[0]; \
			count = count * 2; \
			break; \
		default: \
			ptr = src; \
			break; \
		} \
	}
#endif

#define setup_8bit_elements_indices() \
	uint16_t *ptr; \
	switch (mode) { \
	case GL_QUADS: \
		ptr = gpu_alloc_mapped_temp(count * 3 * sizeof(uint16_t)); \
		for (GLsizei i = 0; i < count / 4; i++) { \
			ptr[i * 6] = src[i * 4]; \
			ptr[i * 6 + 1] = src[i * 4 + 1]; \
			ptr[i * 6 + 2] = src[i * 4 + 3]; \
			ptr[i * 6 + 3] = src[i * 4 + 1]; \
			ptr[i * 6 + 4] = src[i * 4 + 2]; \
			ptr[i * 6 + 5] = src[i * 4 + 3]; \
		} \
		count = (count / 2) * 3; \
		break; \
	case GL_LINE_STRIP: \
		ptr = gpu_alloc_mapped_temp((count - 1) * 2 * sizeof(uint16_t)); \
		for (GLsizei i = 0; i < count - 1; i++) { \
			ptr[i * 2] = src[i]; \
			ptr[i * 2 + 1] = src[i + 1]; \
		} \
		count = (count - 1) * 2; \
		break; \
	case GL_LINE_LOOP: \
		ptr = gpu_alloc_mapped_temp(count * 2 * sizeof(uint16_t)); \
		for (GLsizei i = 0; i < count - 1; i++) { \
			ptr[i * 2] = src[i]; \
			ptr[i * 2 + 1] = src[i + 1]; \
		} \
		ptr[(count - 1) * 2] = src[count - 1]; \
		ptr[(count - 1) * 2 + 1] = src[0]; \
		count = count * 2; \
		break; \
	default: \
		ptr = src; \
		break; \
	}

void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
	THREAD_SAFE()

#ifdef HAVE_DLISTS
	// Enqueueing function to a display list if one is being compiled
	if (_vgl_enqueue_list_func(glDrawArrays, DLIST_FUNC_U32_I32_I32, mode, first, count))
		return;
#endif
#ifndef SKIP_ERROR_HANDLING
	if (phase == MODEL_CREATION) {
		SET_GL_ERROR(GL_INVALID_OPERATION)
	} else if (count <= 0) {
		SET_GL_ERROR_WITH_VALUE(GL_INVALID_VALUE, count)
	}
#endif
	SceGxmPrimitiveType gxm_p;
	gl_primitive_to_gxm(mode, gxm_p, count);
	scene_reset();
	GLboolean is_draw_legal = GL_TRUE;

	if (cur_program != 0)
		is_draw_legal = _glDrawArrays_CustomShadersIMPL(first, count, GL_FALSE);
	else {
		if (!(ffp_vertex_attrib_state & (1 << 0)))
			return;
		_glDrawArrays_FixedFunctionIMPL(first, count);
	}

	if (rio_draw_setup_ready(is_draw_legal))
	{
		uint16_t *ptr;
		switch (mode) {
		case GL_QUADS:
			ptr = default_quads_idx_ptr;
			count = (count / 2) * 3;
			break;
		case GL_LINE_STRIP:
			ptr = default_line_strips_idx_ptr;
			count = (count - 1) * 2;
			break;
		case GL_LINE_LOOP:
			ptr = gpu_alloc_mapped_temp(count * 2 * sizeof(uint16_t));
			vgl_fast_memcpy(ptr, default_line_strips_idx_ptr, (count - 1) * 2 * sizeof(uint16_t));
			ptr[(count - 1) * 2] = count - 1;
			ptr[(count - 1) * 2 + 1] = 0;
			count *= 2;
			break;
		default:
			ptr = default_idx_ptr;
			break;
		}

#ifndef SKIP_ERROR_HANDLING
		if (count > MAX_IDX_NUMBER) {
			vgl_log("%s:%d Attempting to draw a model with glDrawArrays which is too big! Consider increasing MAX_IDX_NUMBER value... (Max requested index: %d)\n", __FILE__, __LINE__, count - 1);
		}
#endif
		sceGxmDraw(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U16, ptr, count);
	}
	restore_polygon_mode(gxm_p);
}

void glMultiDrawArrays(GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount) {
	THREAD_SAFE()

#ifndef SKIP_ERROR_HANDLING
	if (drawcount < 0) {
		SET_GL_ERROR_WITH_VALUE(GL_INVALID_VALUE, drawcount)
	}
#endif

	scene_reset();
	
	GLint lowest = 0x7FFFFFFF;
	GLsizei highest = 0, highest_count = 0;
	for (int i = 0; i < drawcount; i++) {
		if (first[i] < lowest)
			lowest = first[i];
		GLsizei last = first[i] + count[i];
		if (last > highest)
			highest = last;
		if (count[i] > highest_count)
			highest_count = count[i];
	}
	
	SceGxmPrimitiveType gxm_p;
	gl_primitive_to_gxm(mode, gxm_p, highest_count);
	
	uint16_t *idx_ptr;
	switch (mode) {
	case GL_QUADS:
		idx_ptr = default_quads_idx_ptr;
		highest_count = (highest_count / 2) * 3;
		break;
	case GL_LINE_STRIP:
		idx_ptr = default_line_strips_idx_ptr;
		highest_count = (highest_count - 1) * 2;
		break;
	case GL_LINE_LOOP:
		idx_ptr = gpu_alloc_mapped_temp(highest_count * 2 * sizeof(uint16_t));
		vgl_fast_memcpy(idx_ptr, default_line_strips_idx_ptr, (highest_count - 1) * 2 * sizeof(uint16_t));
		idx_ptr[(highest_count - 1) * 2] = highest_count - 1;
		idx_ptr[(highest_count - 1) * 2 + 1] = 0;
		highest_count *= 2;
		break;
	default:
		idx_ptr = default_idx_ptr;
		break;
	}
	
	if (cur_program != 0)
		_glMultiDrawArrays_CustomShadersIMPL(gxm_p, idx_ptr, first, count, lowest, highest, drawcount);
	else {
		if (!(ffp_vertex_attrib_state & (1 << 0)))
			return;
		_glMultiDrawArrays_FixedFunctionIMPL(gxm_p, idx_ptr, first, count, lowest, highest, drawcount);
	}

	restore_polygon_mode(gxm_p);
}

void glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei primcount) {
	THREAD_SAFE()

#ifndef SKIP_ERROR_HANDLING
	if (phase == MODEL_CREATION) {
		SET_GL_ERROR(GL_INVALID_OPERATION)
	} else if (count <= 0) {
		SET_GL_ERROR_WITH_VALUE(GL_INVALID_VALUE, count)
	} else if (cur_program == 0) {
		SET_GL_ERROR(GL_INVALID_OPERATION)
	}
#endif
	SceGxmPrimitiveType gxm_p;
	gl_primitive_to_gxm(mode, gxm_p, count);
	scene_reset();
	GLboolean is_draw_legal = _glDrawArrays_CustomShadersIMPL(first, count, GL_TRUE);

	if (rio_draw_setup_ready(is_draw_legal))
	{
		uint16_t *ptr;
		switch (mode) {
		case GL_QUADS:
			ptr = default_quads_idx_ptr;
			count = (count / 2) * 3;
			break;
		case GL_LINE_STRIP:
			ptr = default_line_strips_idx_ptr;
			count = (count - 1) * 2;
			break;
		case GL_LINE_LOOP:
			ptr = gpu_alloc_mapped_temp(count * 2 * sizeof(uint16_t));
			vgl_fast_memcpy(ptr, default_line_strips_idx_ptr + first * 2, (count - 1) * 2 * sizeof(uint16_t));
			ptr[(count - 1) * 2] = count - 1;
			ptr[(count - 1) * 2 + 1] = 0;
			count *= 2;
			break;
		default:
			ptr = default_idx_ptr;
			break;
		}

#ifndef SKIP_ERROR_HANDLING
		if (count > MAX_IDX_NUMBER) {
			vgl_log("%s:%d Attempting to draw a model with glDrawArraysInstanced which is too big! Consider increasing MAX_IDX_NUMBER value... (Max requested index: %d)\n", __FILE__, __LINE__, count - 1);
		}
#endif
		sceGxmDrawInstanced(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U16, ptr, count * primcount, count);
	}
	restore_polygon_mode(gxm_p);
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *gl_indices) {
	THREAD_SAFE()

#ifdef HAVE_DLISTS
	// Enqueueing function to a display list if one is being compiled
	if (_vgl_enqueue_list_func(glDrawElements, DLIST_FUNC_U32_I32_U32_U32, mode, count, type, gl_indices))
		return;
#endif
#ifndef SKIP_ERROR_HANDLING
	if (type != GL_UNSIGNED_SHORT && type != GL_UNSIGNED_INT && type != GL_UNSIGNED_BYTE) {
		SET_GL_ERROR_WITH_VALUE(GL_INVALID_ENUM, type)
	} else if (phase == MODEL_CREATION) {
		SET_GL_ERROR(GL_INVALID_OPERATION)
	} else if (count <= 0) {
		SET_GL_ERROR_WITH_VALUE(GL_INVALID_VALUE, count)
	}
#endif

	SceGxmPrimitiveType gxm_p;
	gl_primitive_to_gxm(mode, gxm_p, count);
	uint64_t rio_stage=RIO_STAGE_BEGIN();
	scene_reset();
	RIO_STAGE_END(0,rio_stage);
	rio_stage=RIO_STAGE_BEGIN();
	GLboolean is_draw_legal = GL_TRUE;

	vbo *gpu_buf = (vbo *)cur_vao->index_array_unit;
	uint16_t *src = gpu_buf ? (uint16_t *)((uint8_t *)gpu_buf->ptr + (uint32_t)gl_indices) : (uint16_t *)gl_indices;
	
	// sceGxm doesn't support 8bit indices natively, so we internally convert to 16bit
	if (type == GL_UNSIGNED_BYTE) {
		uint16_t *idx16 = gpu_alloc_mapped_temp(count * sizeof(uint16_t));
		uint8_t *idx8 = (uint8_t *)src;
		for (GLsizei i = 0; i < count; i++) {
			idx16[i] = idx8[i];
		}
		src = idx16;
	}
	
	uint32_t rio_first_vertex=0, rio_vertex_count=0;
#ifdef RIO_VERTEX_RANGE_COPY
 ++rio_range_draws;
 /* Restrict rebasing to native, non-instanced U16 triangles with client
  * indices and client vertices. All other entry points remain unchanged. */
 if(cur_program && mode==GL_TRIANGLES && !prim_is_non_native &&
    type==GL_UNSIGNED_SHORT && !gpu_buf && count>0 && vglRioClientRangeEligible()) {
  uint32_t lowest,highest;
  if(rio_vertex_bounds16(src,(size_t)count,&lowest,&highest)) {
   // Reuse the measured upper bound even when trimming is not worthwhile
   // or its allocation fails. Setup otherwise scans this same array again.
   rio_vertex_count=highest+1;
   if(rio_vertex_range_worth_copying(lowest,highest)) {
    uint16_t *rebased=gpu_alloc_mapped_temp((size_t)count*sizeof(*rebased));
    if(rebased) {
     rio_vertex_rebase16(rebased,src,(size_t)count,lowest);
     rio_first_vertex=lowest;rio_vertex_count=highest-lowest+1;
     src=rebased;++rio_range_trimmed;rio_range_saved+=rio_first_vertex;
    }
   }
  }
 }
#endif
 if (cur_program != 0)
  is_draw_legal = _glDrawElements_CustomShadersRangeIMPL(src,count,rio_vertex_count,0,
   type!=GL_UNSIGNED_INT?SCE_GXM_INDEX_SOURCE_INDEX_16BIT:SCE_GXM_INDEX_SOURCE_INDEX_32BIT,rio_first_vertex);
	else {
		if (!(ffp_vertex_attrib_state & (1 << 0)))
			return;
		_glDrawElements_FixedFunctionIMPL(src, count, 0, 0, type != GL_UNSIGNED_INT ? SCE_GXM_INDEX_SOURCE_INDEX_16BIT : SCE_GXM_INDEX_SOURCE_INDEX_32BIT);
	}

	RIO_STAGE_END(1,rio_stage);
	if (rio_draw_setup_ready(is_draw_legal))
	{
		if (rio_first_vertex) {
   /* The rebased buffer already belongs to the ordinary temporary GPU pool. */
   rio_stage=RIO_STAGE_BEGIN();
   sceGxmDraw(gxm_context,gxm_p,SCE_GXM_INDEX_FORMAT_U16,src,count);
   RIO_STAGE_END(3,rio_stage);
  } else if (type == GL_UNSIGNED_SHORT) {
			rio_stage=RIO_STAGE_BEGIN();
			setup_elements_indices(uint16_t);
			RIO_STAGE_END(2,rio_stage);
			rio_stage=RIO_STAGE_BEGIN();
			sceGxmDraw(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U16, ptr, count);
			RIO_STAGE_END(3,rio_stage);
		} else if (type == GL_UNSIGNED_INT) {
			rio_stage=RIO_STAGE_BEGIN();
			setup_elements_indices(uint32_t);
			RIO_STAGE_END(2,rio_stage);
			rio_stage=RIO_STAGE_BEGIN();
			sceGxmDraw(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U32, ptr, count);
			RIO_STAGE_END(3,rio_stage);
		} else {
			rio_stage=RIO_STAGE_BEGIN();
			setup_8bit_elements_indices();
			RIO_STAGE_END(2,rio_stage);
			rio_stage=RIO_STAGE_BEGIN();
			sceGxmDraw(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U16, ptr, count);
			RIO_STAGE_END(3,rio_stage);
		}
	}
	restore_polygon_mode(gxm_p);
}

void glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const GLvoid *gl_indices, GLint baseVertex) {
	THREAD_SAFE()

#ifndef SKIP_ERROR_HANDLING
	if (type != GL_UNSIGNED_SHORT && type != GL_UNSIGNED_INT) {
		SET_GL_ERROR_WITH_VALUE(GL_INVALID_ENUM, type)
	} else if (phase == MODEL_CREATION) {
		SET_GL_ERROR(GL_INVALID_OPERATION)
	} else if (count <= 0) {
		SET_GL_ERROR_WITH_VALUE(GL_INVALID_VALUE, count)
	}
#endif

	SceGxmPrimitiveType gxm_p;
	gl_primitive_to_gxm(mode, gxm_p, count);
	scene_reset();
	GLboolean is_draw_legal = GL_TRUE;

	vbo *gpu_buf = (vbo *)cur_vao->index_array_unit;
	uint16_t *src = gpu_buf ? (uint16_t *)((uint8_t *)gpu_buf->ptr + (uint32_t)gl_indices) : (uint16_t *)gl_indices;
	if (cur_program != 0)
		is_draw_legal = _glDrawElements_CustomShadersIMPL(src, count, 0, baseVertex, type == GL_UNSIGNED_SHORT ? SCE_GXM_INDEX_SOURCE_INDEX_16BIT : SCE_GXM_INDEX_SOURCE_INDEX_32BIT);
	else {
		if (!(ffp_vertex_attrib_state & (1 << 0)))
			return;
		_glDrawElements_FixedFunctionIMPL(src, count, 0, baseVertex, type == GL_UNSIGNED_SHORT ? SCE_GXM_INDEX_SOURCE_INDEX_16BIT : SCE_GXM_INDEX_SOURCE_INDEX_32BIT);
	}

	if (rio_draw_setup_ready(is_draw_legal))
	{
		if (type == GL_UNSIGNED_INT) {
			setup_elements_indices(uint32_t);
			sceGxmDraw(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U32 + baseVertex, ptr, count);
		}
		else
		{
			setup_elements_indices(uint16_t);
			sceGxmDraw(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U16 + baseVertex, ptr, count);
		}
	}
	restore_polygon_mode(gxm_p);
}

void glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *gl_indices) {
	THREAD_SAFE()

#ifndef SKIP_ERROR_HANDLING
	if (type != GL_UNSIGNED_SHORT && type != GL_UNSIGNED_INT) {
		SET_GL_ERROR_WITH_VALUE(GL_INVALID_ENUM, type)
	} else if (phase == MODEL_CREATION) {
		SET_GL_ERROR(GL_INVALID_OPERATION)
	} else if (count <= 0) {
		SET_GL_ERROR_WITH_VALUE(GL_INVALID_VALUE, count)
	} else if (end < start) {
		SET_GL_ERROR(GL_INVALID_VALUE)
	}
#endif

	SceGxmPrimitiveType gxm_p;
	gl_primitive_to_gxm(mode, gxm_p, count);
	scene_reset();
	GLboolean is_draw_legal = GL_TRUE;

	vbo *gpu_buf = (vbo *)cur_vao->index_array_unit;
	uint16_t *src = gpu_buf ? (uint16_t *)((uint8_t *)gpu_buf->ptr + (uint32_t)gl_indices) : (uint16_t *)gl_indices;
	if (cur_program != 0)
		is_draw_legal = _glDrawElements_CustomShadersIMPL(src, count, end + 1, 0, type == GL_UNSIGNED_SHORT ? SCE_GXM_INDEX_SOURCE_INDEX_16BIT : SCE_GXM_INDEX_SOURCE_INDEX_32BIT);
	else {
		if (!(ffp_vertex_attrib_state & (1 << 0)))
			return;
		_glDrawElements_FixedFunctionIMPL(src, count, end + 1, 0, type == GL_UNSIGNED_SHORT ? SCE_GXM_INDEX_SOURCE_INDEX_16BIT : SCE_GXM_INDEX_SOURCE_INDEX_32BIT);
	}

	if (rio_draw_setup_ready(is_draw_legal))
	{
		if (type == GL_UNSIGNED_INT) {
			setup_elements_indices(uint32_t);
			sceGxmDraw(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U32, ptr, count);
		}
		else
		{
			setup_elements_indices(uint16_t);
			sceGxmDraw(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U16, ptr, count);
		}
	}
	restore_polygon_mode(gxm_p);
}

void glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, void *gl_indices, GLint baseVertex) {
	THREAD_SAFE()

#ifndef SKIP_ERROR_HANDLING
	if (type != GL_UNSIGNED_SHORT && type != GL_UNSIGNED_INT) {
		SET_GL_ERROR_WITH_VALUE(GL_INVALID_ENUM, type)
	} else if (phase == MODEL_CREATION) {
		SET_GL_ERROR(GL_INVALID_OPERATION)
	} else if (count <= 0) {
		SET_GL_ERROR_WITH_VALUE(GL_INVALID_VALUE, count)
	}
#endif

	SceGxmPrimitiveType gxm_p;
	gl_primitive_to_gxm(mode, gxm_p, count);
	scene_reset();
	GLboolean is_draw_legal = GL_TRUE;

	vbo *gpu_buf = (vbo *)cur_vao->index_array_unit;
	uint16_t *src = gpu_buf ? (uint16_t *)((uint8_t *)gpu_buf->ptr + (uint32_t)gl_indices) : (uint16_t *)gl_indices;
	if (cur_program != 0)
		is_draw_legal = _glDrawElements_CustomShadersIMPL(src, count, end + baseVertex + 1, baseVertex, type == GL_UNSIGNED_SHORT ? SCE_GXM_INDEX_SOURCE_INDEX_16BIT : SCE_GXM_INDEX_SOURCE_INDEX_32BIT);
	else {
		if (!(ffp_vertex_attrib_state & (1 << 0)))
			return;
		_glDrawElements_FixedFunctionIMPL(src, count, end + baseVertex + 1, baseVertex, type == GL_UNSIGNED_SHORT ? SCE_GXM_INDEX_SOURCE_INDEX_16BIT : SCE_GXM_INDEX_SOURCE_INDEX_32BIT);
	}

	if (rio_draw_setup_ready(is_draw_legal))
	{
		if (type == GL_UNSIGNED_INT) {
			setup_elements_indices(uint32_t)
			sceGxmDraw(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U32 + baseVertex, ptr, count);
		}
		else
		{
			setup_elements_indices(uint16_t)
			sceGxmDraw(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U16 + baseVertex, ptr, count);
		}
	}
	restore_polygon_mode(gxm_p);
}

void glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void *gl_indices, GLsizei primcount) {
	THREAD_SAFE()

#ifndef SKIP_ERROR_HANDLING
	if (type != GL_UNSIGNED_SHORT && type != GL_UNSIGNED_INT) {
		SET_GL_ERROR_WITH_VALUE(GL_INVALID_ENUM, type)
	} else if (phase == MODEL_CREATION) {
		SET_GL_ERROR(GL_INVALID_OPERATION)
	} else if (count <= 0) {
		SET_GL_ERROR_WITH_VALUE(GL_INVALID_VALUE, count)
	} else if (cur_program == 0) {
		SET_GL_ERROR(GL_INVALID_OPERATION)
	}
#endif

	SceGxmPrimitiveType gxm_p;
	gl_primitive_to_gxm(mode, gxm_p, count);
	scene_reset();

	vbo *gpu_buf = (vbo *)cur_vao->index_array_unit;
	uint16_t *src = gpu_buf ? (uint16_t *)((uint8_t *)gpu_buf->ptr + (uint32_t)gl_indices) : (uint16_t *)gl_indices;
	GLboolean is_draw_legal = _glDrawElements_CustomShadersIMPL(src, count, 0, 0, type == GL_UNSIGNED_SHORT ? SCE_GXM_INDEX_SOURCE_INSTANCE_16BIT : SCE_GXM_INDEX_SOURCE_INSTANCE_32BIT);

	if (rio_draw_setup_ready(is_draw_legal))
	{
		if (type == GL_UNSIGNED_INT) {
			setup_elements_indices(uint32_t);
			sceGxmDrawInstanced(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U32, ptr, count * primcount, count);
		}
		else
		{
			setup_elements_indices(uint16_t);
			sceGxmDrawInstanced(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U16, ptr, count * primcount, count);
		}
	}
	restore_polygon_mode(gxm_p);
}

void vglDrawObjects(GLenum mode, GLsizei count) {
	THREAD_SAFE()

#ifdef ENABLE_LEGACY_PIPELINE
#ifndef SKIP_ERROR_HANDLING
	if (phase == MODEL_CREATION) {
		SET_GL_ERROR(GL_INVALID_OPERATION)
	} else if (count <= 0) {
		SET_GL_ERROR_WITH_VALUE(GL_INVALID_VALUE, count)
	}
#endif

	SceGxmPrimitiveType gxm_p;
	gl_primitive_to_gxm(mode, gxm_p, count);
	scene_reset();

	texture_unit *tex_unit = &texture_units[0];
	if (cur_program != 0) {
		_vglDrawObjects_CustomShadersIMPL();
		sceGxmDraw(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U16, index_object, count);
	} else if (ffp_vertex_attrib_state & (1 << FFP_ATTRIB_POSITION)) {
		reload_ffp_shaders(NULL, NULL, SCE_GXM_INDEX_SOURCE_INDEX_16BIT);
		if (ffp_vertex_attrib_state & (1 << FFP_ATTRIB_TEX0)) {
			if (texture_slots[tex_unit->tex_id[0]].status != TEX_VALID)
				return;
#ifndef TEXTURES_SPEEDHACK
			texture_slots[tex_unit->tex_id[0]].last_frame = vgl_framecount;
#endif
			sceGxmSetFragmentTexture(gxm_context, 0, &texture_slots[tex_unit->tex_id[0]].gxm_tex);
			sceGxmSetVertexStream(gxm_context, 1, texture_object);
			if (ffp_vertex_num_params > 2)
				sceGxmSetVertexStream(gxm_context, 2, color_object);
		} else if (ffp_vertex_num_params > 1)
			sceGxmSetVertexStream(gxm_context, 1, color_object);
		sceGxmSetVertexStream(gxm_context, 0, vertex_object);
		sceGxmDraw(gxm_context, gxm_p, SCE_GXM_INDEX_FORMAT_U16, index_object, count);
	}

	restore_polygon_mode(gxm_p);
#endif
}

void vglIndexPointerDefault() {
	THREAD_SAFE()

	index_object = default_idx_ptr;
}
