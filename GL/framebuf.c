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
#include "gli.h"

/*
 * Data Definitions
 */

static GLint scissor_x, scissor_y;
static GLsizei scissor_width, scissor_height;
static enum gli_Func alpha_func, stencil_func, depth_func;
static GLclampx alpha_ref;
static GLint stencil_ref;
static GLuint stencil_mask;
static struct {
	enum gli_StencilOp fail, zfail, zpass;
} stencil_ops;
static GLclampx clear_colors[4];
static GLclampx clear_depth;
static GLint clear_stencil;
static GLboolean color_mask[4], depth_mask;

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
	scissor_x = scissor_y = 0;
	scissor_width = SCREEN_WIDTH;
	scissor_height = SCREEN_HEIGHT;
	alpha_func = stencil_func = GL_ALWAYS;
	depth_func = GL_LESS;
	alpha_ref = stencil_ref = 0;
	stencil_mask = ~0;
	stencil_ops.fail = stencil_ops.zfail = stencil_ops.zpass = GL_KEEP;
	clear_colors[0] = clear_colors[1] = clear_colors[2] = clear_colors[3] = 0;
	clear_depth = 0x10000;
	clear_stencil = 0;
	color_mask[0] = color_mask[1] = color_mask[2] = color_mask[3] = GL_TRUE;
	depth_mask = GL_TRUE;
	return 0;
}

extern inline void gli_framebuf_end(void);

void glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
	if (width < 0 || height < 0) {
		return gli_set_error(GL_INVALID_VALUE);
	}
	scissor_x = x;
	scissor_y = y;
	scissor_width = width;
	scissor_height = height;
}

void glAlphaFuncx(GLenum func, GLclampx ref)
{
	if (func < GL_NEVER || func > GL_ALWAYS) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	CLAMP(ref, 0, 0x10000);
	alpha_func = func;
	alpha_ref = ref;
}

void glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
	if (func < GL_NEVER || func > GL_ALWAYS) {
		return gli_set_error(GL_INVALID_ENUM);
	}
#	if GLI_STENCIL_BITS > 0
	CLAMP(ref, 0, (1<<(GLI_STENCIL_BITS-1)));
#	else
	ref = 0;
#	endif
	stencil_func = func;
	stencil_ref = ref;
	stencil_mask = mask;
}

void glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
	if (!valid_stencil_op(fail) || !valid_stencil_op(zfail) || !valid_stencil_op(zpass)) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	stencil_ops.fail = fail;
	stencil_ops.zfail = zfail;
	stencil_ops.zpass = zpass;
}

void glDepthFunc(GLenum func)
{
	if (func < GL_NEVER || func > GL_ALWAYS) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	depth_func = func;
}

void glBlendFunc(GLenum sfactor, GLenum dfactor)
{
	// TODO
	(void)sfactor;
	(void)dfactor;
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
	// TODO
}

void glClearColorx(GLclampx red, GLclampx green, GLclampx blue, GLclampx alpha)
{
	CLAMP(red, 0, 0x10000);
	CLAMP(green, 0, 0x10000);
	CLAMP(blue, 0, 0x10000);
	CLAMP(alpha, 0, 0x10000);
	clear_colors[0] = red;
	clear_colors[1] = green;
	clear_colors[2] = blue;
	clear_colors[3] = alpha;
}

void glClearDepthx(GLclampx depth)
{
	CLAMP(depth, 0, 0x10000);
	clear_depth = depth;
}

void glClearStencil(GLint s)
{
#  if GLI_STENCIL_BITS > 0
	clear_stencil = s & ((1<<GLI_STENCIL_BITS)-1);
#	else
	(void)s;
#	endif
}

void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	color_mask[0] = red;
	color_mask[1] = green;
	color_mask[2] = blue;
	color_mask[3] = alpha;
}

void glDepthMask(GLboolean flag)
{
	depth_mask = flag;
}

void glStencilMask(GLuint mask)
{
	stencil_mask = mask;
}

void glSampleCoveragex(GLclampx value, GLboolean invert)
{
	// TODO
	(void)value;
	(void)invert;
}
