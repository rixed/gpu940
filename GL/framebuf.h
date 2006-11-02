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
#ifndef GL_FRAMEBUF_H_061010
#define GL_FRAMEBUF_H_061010

extern GLint gli_scissor_x, gli_scissor_y;
extern GLsizei gli_scissor_width, gli_scissor_height;
extern enum gli_Func gli_alpha_func, gli_stencil_func, gli_depth_func;
extern GLclampx gli_alpha_ref;
extern GLint gli_stencil_ref;
extern GLuint gli_stencil_mask;
extern struct gli_stencil_ops {
	enum gli_StencilOp fail, zfail, zpass;
} gli_stencil_ops;
extern GLclampx gli_clear_colors[4];
extern GLclampx gli_clear_depth;
extern GLint gli_clear_stencil;
extern GLboolean gli_color_mask[4], gli_color_mask_all, gli_depth_mask;

int gli_framebuf_begin(void);
static inline void gli_framebuf_end(void) {}

#endif
