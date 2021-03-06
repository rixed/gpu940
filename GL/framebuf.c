/* This file is part of gpu940.
 *
 * Copyright (C) 2006 Cedric Cellier.
 *
 * Gpu940 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * Gpu940 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gpu940; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <limits.h>
#include "gli.h"

/*
 * Data Definitions
 */

GLint gli_scissor_x, gli_scissor_y;
GLsizei gli_scissor_width, gli_scissor_height;
enum gli_Func gli_alpha_func, gli_stencil_func, gli_depth_func;
GLclampx gli_alpha_ref;
GLint gli_stencil_ref;
GLuint gli_stencil_mask;
struct gli_stencil_ops gli_stencil_ops;
GLclampx gli_clear_colors[4];
GLclampx gli_clear_depth;
GLint gli_clear_stencil;
GLboolean gli_color_mask[4], gli_color_mask_all, gli_depth_mask;
enum gli_BlendFunc gli_sfactor, gli_dfactor;

/*
 * Private Functions
 */

static bool valid_stencil_op(GLenum op)
{
	return (op >= GL_KEEP && op <= GL_INVERT) || op == GL_REPLACE || op == GL_ZERO || op == GL_INVERT;
}

/*
 * Public Functions
 */

int gli_framebuf_begin(void)
{
	gli_scissor_x = gli_scissor_y = 0;
	gli_scissor_width = SCREEN_WIDTH;
	gli_scissor_height = SCREEN_HEIGHT;
	gli_alpha_func = gli_stencil_func = GL_ALWAYS;
	gli_depth_func = GL_LESS;
	gli_alpha_ref = gli_stencil_ref = 0;
	gli_stencil_mask = ~0;
	gli_stencil_ops.fail = gli_stencil_ops.zfail = gli_stencil_ops.zpass = GL_KEEP;
	gli_clear_colors[0] = gli_clear_colors[1] = gli_clear_colors[2] = gli_clear_colors[3] = 0;
	gli_clear_depth = 0x10000;
	gli_clear_stencil = 0;
	gli_color_mask[0] = gli_color_mask[1] = gli_color_mask[2] = gli_color_mask[3] = gli_color_mask_all = GL_TRUE;
	gli_depth_mask = GL_TRUE;
	gli_sfactor = GL_ONE;
	gli_dfactor = GL_ZERO;
	return 0;
}

extern inline void gli_framebuf_end(void);

void glDrawBuffer(GLenum buf)
{
	if ((buf < GL_NONE || buf > GL_AUX0) && buf != GL_FRONT && buf != GL_BACK && buf != GL_FRONT_AND_BACK) {
		return gli_set_error(GL_INVALID_VALUE);
	}
	// TODO
}

void glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
	if (width < 0 || height < 0) {
		return gli_set_error(GL_INVALID_VALUE);
	}
	gli_scissor_x = x;
	gli_scissor_y = y;
	gli_scissor_width = width;
	gli_scissor_height = height;
}

void glAlphaFuncx(GLenum func, GLclampx ref)
{
	if (/*func < GL_NEVER ||*/ func > GL_ALWAYS) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	CLAMP(ref, 0, 0xFFFF);
	gli_alpha_func = func;
	gli_alpha_ref = ref;
}

void glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
	if (/*func < GL_NEVER ||*/ func > GL_ALWAYS) {
		return gli_set_error(GL_INVALID_ENUM);
	}
#	if GLI_STENCIL_BITS > 0
	CLAMP(ref, 0, (1<<(GLI_STENCIL_BITS-1))-1);
#	else
	ref = 0;
#	endif
	gli_stencil_func = func;
	gli_stencil_ref = ref;
	gli_stencil_mask = mask;
}

void glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
	if (!valid_stencil_op(fail) || !valid_stencil_op(zfail) || !valid_stencil_op(zpass)) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	gli_stencil_ops.fail = fail;
	gli_stencil_ops.zfail = zfail;
	gli_stencil_ops.zpass = zpass;
}

void glDepthFunc(GLenum func)
{
	if (/*func < GL_NEVER ||*/ func > GL_ALWAYS) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	gli_depth_func = func;
}

void glBlendFunc(GLenum sfactor, GLenum dfactor)
{
	if (
		sfactor != GL_ZERO && sfactor != GL_ONE && sfactor != GL_DST_COLOR &&
		sfactor != GL_ONE_MINUS_DST_COLOR && sfactor != GL_SRC_ALPHA && sfactor != GL_ONE_MINUS_SRC_ALPHA &&
		sfactor != GL_DST_ALPHA && sfactor != GL_ONE_MINUS_DST_ALPHA && sfactor != GL_SRC_ALPHA_SATURATE
	) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	if (
		dfactor != GL_ZERO && dfactor != GL_ONE && dfactor != GL_SRC_COLOR &&
		dfactor != GL_ONE_MINUS_SRC_COLOR && dfactor != GL_SRC_ALPHA && dfactor != GL_ONE_MINUS_SRC_ALPHA &&
		dfactor != GL_DST_ALPHA && dfactor != GL_ONE_MINUS_DST_ALPHA
	) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	gli_sfactor = sfactor;
	gli_dfactor = dfactor;
	// Anyway, the only blending function handled by gpu940 is GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
	// with a constant SRC_ALPHA (the alpha values are not stored in the framebuffer, thus we are
	// left with src_alpha only). Also, we can make use of the key_color to clear incomming pixels
	// with 0 alpha out, so that we have 2 possible alpha values : 0 and another one.
}

void glLogicOp(GLenum opcode)
{
	// TODO
	(void)opcode;
}

void glClear(GLbitfield mask)
{
	if (mask & ~(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT)) {
		return gli_set_error(GL_INVALID_VALUE);
	}
	if (mask & GL_COLOR_BUFFER_BIT) {
		gli_clear(gpuOutBuffer, gli_clear_colors);
	}
	if (mask & GL_DEPTH_BUFFER_BIT) {
		int32_t max = INT32_MAX;
		gli_clear(gpuZBuffer, &max/*gli_clear_depth*/);
	}
}

void glClearColorx(GLclampx red, GLclampx green, GLclampx blue, GLclampx alpha)
{
	CLAMP(red, 0, 0xFFFF);
	CLAMP(green, 0, 0xFFFF);
	CLAMP(blue, 0, 0xFFFF);
	CLAMP(alpha, 0, 0xFFFF);
	gli_clear_colors[0] = red;
	gli_clear_colors[1] = green;
	gli_clear_colors[2] = blue;
	gli_clear_colors[3] = alpha;
}

void glClearDepthx(GLclampx depth)
{
	CLAMP(depth, 0, 0x10000);
	gli_clear_depth = depth;
}

void glClearStencil(GLint s)
{
#  if GLI_STENCIL_BITS > 0
	gli_clear_stencil = s & ((1<<GLI_STENCIL_BITS)-1);
#	else
	(void)s;
#	endif
}

void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	gli_color_mask[0] = red;
	gli_color_mask[1] = green;
	gli_color_mask[2] = blue;
	gli_color_mask[3] = alpha;
	gli_color_mask_all = red || green || blue;
}

void glDepthMask(GLboolean flag)
{
	gli_depth_mask = flag;
}

void glStencilMask(GLuint mask)
{
	gli_stencil_mask = mask;
}

void glSampleCoveragex(GLclampx value, GLboolean invert)
{
	// TODO
	(void)value;
	(void)invert;
}
