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

static struct matrix_stack ms[3];	// modelview, projection, texture
static enum gli_MatrixMode matrix_mode;

/*
 * Private Functions
 */

static void compute_ab(FixMat *m)
{
	for (unsigned i=0; i<3; i++) {
		m->ab[i] = Fix_mul(m->rot[0][i], m->rot[1][i]);
	}
}

static void do_matrix(FixMat *mat, GLfixed const *m)
{
	mat->rot[0][0] = m[0];
	mat->rot[0][1] = m[1];
	mat->rot[0][2] = m[2];
	mat->rot[1][0] = m[4];
	mat->rot[1][1] = m[5];
	mat->rot[1][2] = m[6];
	mat->rot[2][0] = m[8];
	mat->rot[2][1] = m[9];
	mat->rot[2][2] = m[10];
	mat->trans[0] = m[3];
	mat->trans[1] = m[7];
	mat->trans[2] = m[11];
	compute_ab(mat);
}

static void mult_matrix(FixMat *mat)
{
	FixMat *const dest = ms[matrix_mode].mat + ms[matrix_mode].top;
	FixMat_x_Mat2(dest, mat);
}

static void scale_vec(int32_t vec[3], int32_t s)
{
	for (unsigned i=0; i<3; i++) {
		vec[i] = Fix_mul(vec[i], s);
	}
}

/*
 * Public Functions
 */

int gli_transfo_begin(void)
{
	ms[GL_MODELVIEW].size = GLI_MAX_MODELVIEW_STACK_DEPTH;
	ms[GL_MODELVIEW].top = 0;
	ms[GL_MODELVIEW].mat = malloc(sizeof(*ms[GL_MODELVIEW].mat)*GLI_MAX_MODELVIEW_STACK_DEPTH);
	if (! ms[GL_MODELVIEW].mat) return -1;
	ms[GL_MODELVIEW].mat[0] = matrix_id;
	ms[GL_PROJECTION].size = GLI_MAX_PROJECTION_STACK_DEPTH;
	ms[GL_PROJECTION].top = 0;
	ms[GL_PROJECTION].mat = malloc(sizeof(*ms[GL_PROJECTION].mat)*GLI_MAX_PROJECTION_STACK_DEPTH);
	if (! ms[GL_PROJECTION].mat) return -1;
	ms[GL_PROJECTION].mat[0] = matrix_id;
	ms[GL_TEXTURE].size = GLI_MAX_TEXTURE_STACK_DEPTH;
	ms[GL_TEXTURE].top = 0;
	ms[GL_TEXTURE].mat = malloc(sizeof(*ms[GL_TEXTURE].mat)*GLI_MAX_TEXTURE_STACK_DEPTH);
	if (! ms[GL_TEXTURE].mat) return -1;
	ms[GL_TEXTURE].mat[0] = matrix_id;
	matrix_mode = GL_MODELVIEW;
	Fix_trig_init();
	return 0;
}

extern inline void gli_state_end(void);

void glMatrixMode(GLenum mode)
{
	if (mode != GL_MODELVIEW && mode != GL_PROJECTION && mode != GL_TEXTURE) {
		gli_set_error(GL_INVALID_ENUM);
		return;
	}
	matrix_mode = mode;
}

void glPushMatrix(void)
{
	if (ms[matrix_mode].top >= ms[matrix_mode].size) {
		gli_set_error(GL_STACK_OVERFLOW);
		return;
	}
	ms[matrix_mode].mat[ms[matrix_mode].top+1] = ms[matrix_mode].mat[ms[matrix_mode].top];
	ms[matrix_mode].top ++;
}

void glPopMatrix(void)
{
	if (ms[matrix_mode].top == 0) {
		gli_set_error(GL_STACK_UNDERFLOW);
		return;
	}
	ms[matrix_mode].top --;
}

void glLoadMatrixx(GLfixed const *m)
{
	FixMat *const mat = ms[matrix_mode].mat + ms[matrix_mode].top;
	do_matrix(&mat, m);
}

void glLoadIdentity(void)
{
	ms[matrix_mode].mat[ ms[matrix_mode].top ] = matrix_id;
}

void glRotatex(GLfixed angle, GLfixed x, GLfixed y, GLfixed z)
{
	int32_t const c = Fix_cos(angle);
	int32_t const s = Fix_sin(angle);
	int32_t v[3] = { x, y, z };
	Fix_normalize(v);
	FixMat m;
	int32_t const xy_ = Fix_mul( Fix_mul(v[0], v[1]), 0x10000 - c );
	int32_t const yz_ = Fix_mul( Fix_mul(v[1], v[2]), 0x10000 - c );
	int32_t const xz_ = Fix_mul( Fix_mul(v[0], v[2]), 0x10000 - c );
	int32_t const xs = Fix_mul(v[0], s);
	int32_t const ys = Fix_mul(v[1], s);
	int32_t const zs = Fix_mul(v[2], s);
	m.rot[0][0] = Fix_mul( Fix_mul(v[0], v[0]), 0x10000 - c ) + c;
	m.rot[0][1] = xy_ + zs;
	m.rot[0][2] = xz_ - ys;
	m.rot[1][0] = xy_ - zs;
	m.rot[1][1] = Fix_mul( Fix_mul(v[1], v[1]), 0x10000 - c ) + c;
	m.rot[1][2] = yz_ + xs;
	m.rot[2][0] = xz_ + ys;
	m.rot[2][1] = yz_ - xs;
	m.rot[2][2] = Fix_mul( Fix_mul(v[2], v[2]), 0x10000 - c ) + c;
	m.trans[0] = 0;
	m.trans[1] = 0;
	m.trans[2] = 0;
	compute_ab(&m);
	mult_matrix(&m);
}

void glMultMatrixx(const GLfixed * m)
{
	FixMat mat;
	do_matrix(&mat, m);
	mult_matrix(&mat);
}

void glScalex(GLfixed x, GLfixed y, GLfixed z)
{
	FixMat *const mat = ms[matrix_mode].mat + ms[matrix_mode].top;
	scale_vec(mat->rot[0], x);
	scale_vec(mat->rot[1], y);
	scale_vec(mat->rot[2], z);
}

void glTranslatex(GLfixed x, GLfixed y, GLfixed z)
{
	FixMat m = matrix_id;	// TODO: May not work, and need to be faster.
	m.trans[0] = x;
	m.trans[1] = y;
	m.trans[2] = z;
	mult_matrix(&m);
}

void glFrustumx(GLfixed left, GLfixed right, GLfixed bottom, GLfixed top, GLfixed near, GLfixed far)
{
	FixMat m;
	GLfixed m[16] = ...
}
