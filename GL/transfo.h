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
#ifndef GL_TRANSFO_H_061009
#define GL_TRANSFO_H_061009

struct matrix_stack {
	unsigned top;
	unsigned size;
	GLfixed (*mat)[16];
};

extern GLint gli_viewport_x, gli_viewport_y;
extern GLsizei gli_viewport_width, gli_viewport_height;
extern GLfixed gli_depth_range_near, gli_depth_range_far;

int gli_transfo_begin(void);
static inline void gli_transfo_end(void) {}

int gli_matrix_stack_ctor(struct matrix_stack *mt, unsigned size);
void gli_matrix_stack_dtor(struct matrix_stack *mt);
void gli_multmatrix(enum gli_MatrixMode mode, int32_t dest[4], int32_t const src[4]);
void gli_multmatrixU(enum gli_MatrixMode mode, int32_t dest[3], int32_t const src[3]);

#endif
