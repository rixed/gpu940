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
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "fixmath.h"

/*
 * Data Definitions
 */

struct gli_matrix_stack modelview_ms, projection_ms;
static enum gli_MatrixMode matrix_mode;
static GLfixed const gli_matrix_id[16] = {
	0x10000, 0, 0, 0,
	0, 0x10000, 0, 0,
	0, 0, 0x10000, 0,
	0, 0, 0, 0x10000,
};
GLfixed gli_depth_range_near;
GLfixed gli_depth_range_far;
GLint gli_viewport_x, gli_viewport_y;
GLsizei gli_viewport_width, gli_viewport_height;

/*
 * Private Functions
 */

static struct gli_matrix_stack *get_ms(enum gli_MatrixMode mode)
{
	switch (mode) {
		case GL_MODELVIEW:
			return &modelview_ms;
		case GL_PROJECTION:
			return &projection_ms;
		case GL_TEXTURE:
			return &gli_texture_unit.ms;
		default:
			assert(0);
			return NULL;
	}
}

static void compute_ab(FixMat *m)
{
	for (unsigned i=0; i<3; i++) {
		m->ab[i] = Fix_mul(m->rot[0][i], m->rot[1][i]);
	}
}

static void GCCunused do_matrix(FixMat *mat, GLfixed const *m)
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

static void mult_matrix(GLfixed const *mat)
{
	struct gli_matrix_stack *const ms = get_ms(matrix_mode);
	GLfixed (*const dest)[16] = ms->mat + ms->top;
	GLfixed res[16];
	for (unsigned c=0; c<4; c++) {
		for (unsigned r=0; r<4; r++) {
			res[4*c+r] = 0;
			for (unsigned i=0; i<4; i++) {
				res[4*c+r] += Fix_mul((*dest)[4*i+r], mat[4*c+i]);
			}
		}
	}
	memcpy(dest, res, sizeof(*dest));
}

static void scale_vec(int32_t vec[4], int32_t s)
{
	for (unsigned i=0; i<4; i++) {
		vec[i] = Fix_mul(vec[i], s);
	}
}

static void frustum_ortho(int frustrum, GLfixed left, GLfixed right, GLfixed bottom, GLfixed top, GLfixed near, GLfixed far)
{
	GLfixed mat[16];
	int32_t rli = Fix_inv(right-left);
	int32_t tbi = Fix_inv(top-bottom);
	int32_t fni = Fix_inv(far-near);
	mat[1] = 0;
	mat[2] = 0;
	mat[3] = 0;
	mat[4] = 0;
	mat[6] = 0;
	mat[7] = 0;
	if (frustrum) {
		mat[0] = Fix_mul(near, rli) << 1;
		mat[5] = Fix_mul(near, tbi) << 1;
		mat[8] = Fix_mul(right+left, rli);
		mat[9] = Fix_mul(top+bottom, tbi);
		mat[10] = -Fix_mul(far+near, fni);
		mat[11] = -1 << 16;
		mat[12] = 0;
		mat[13] = 0;
		mat[14] = -Fix_mul( Fix_mul(far, near), fni<<1);
		mat[15] = 0;
	} else {	// ortho
		mat[0] = rli << 1;
		mat[5] = tbi << 1;
		mat[8] = 0;
		mat[9] = 0;
		mat[10] = -fni << 1;
		mat[11] = 0;
		mat[12] = -Fix_mul(right+left, rli);
		mat[13] = -Fix_mul(top+bottom, tbi);
		mat[14] = -Fix_mul(far+near, fni);
		mat[15] = 1 << 16;
	}
	mult_matrix(mat);
}

/*
 * Public Functions
 */

int gli_matrix_stack_ctor(struct gli_matrix_stack *ms, unsigned size)
{
	assert(size > 0);
	ms->size = size;
	ms->top = 0;
	ms->mat = malloc(sizeof(*ms->mat)*size);
	if (! ms->mat) return -1;
	memcpy(ms->mat, gli_matrix_id, sizeof(*ms->mat));
	return 0;
}

void gli_matrix_stack_dtor(struct gli_matrix_stack *ms)
{
	return free(ms->mat);
}

int gli_transfo_begin(void)
{
	if (0 != gli_matrix_stack_ctor(&modelview_ms, GLI_MAX_MODELVIEW_STACK_DEPTH)) return -1;
	if (0 != gli_matrix_stack_ctor(&projection_ms, GLI_MAX_PROJECTION_STACK_DEPTH)) return -1;
	matrix_mode = GL_MODELVIEW;
	gli_depth_range_near = 0;
	gli_depth_range_far = 0x10000;
	gli_viewport_x = 0;
	gli_viewport_y = 0;
	gli_viewport_width = SCREEN_WIDTH;
	gli_viewport_height = SCREEN_HEIGHT;
	Fix_trig_init();
	return 0;
}

extern inline void gli_state_end(void);

void gli_multmatrix(enum gli_MatrixMode mode, int32_t dest[4], int32_t const src[4])
{
	struct gli_matrix_stack *const ms = get_ms(mode);
	GLfixed (*const topmat)[16] = ms->mat + ms->top;
	for (unsigned i=0; i<4; i++) {
		dest[i] = 0;
		for (unsigned j=0; j<4; j++) {
			dest[i] += Fix_mul((*topmat)[i + 4*j], src[j]);
		}
	}
}

void gli_multmatrixU(enum gli_MatrixMode mode, int32_t dest[3], int32_t const src[3])
{
	struct gli_matrix_stack *const ms = get_ms(mode);
	GLfixed (*const topmat)[16] = ms->mat + ms->top;
	for (unsigned i=0; i<3; i++) {
		dest[i] = 0;
		for (unsigned j=0; j<3; j++) {
			dest[i] += Fix_mul((*topmat)[i + 4*j], src[j]);
		}
	}
}

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
	struct gli_matrix_stack *const ms = get_ms(matrix_mode);
	if (ms->top >= ms->size) {
		gli_set_error(GL_STACK_OVERFLOW);
		return;
	}
	memcpy(ms->mat[ms->top+1], ms->mat[ms->top], sizeof(*ms->mat));
	ms->top ++;
}

void glPopMatrix(void)
{
	struct gli_matrix_stack *const ms = get_ms(matrix_mode);
	if (ms->top == 0) {
		gli_set_error(GL_STACK_UNDERFLOW);
		return;
	}
	ms->top --;
}

void glLoadMatrixx(GLfixed const *m)
{
	struct gli_matrix_stack *const ms = get_ms(matrix_mode);
	GLfixed (*const mat)[16] = ms->mat + ms->top;
	memcpy(mat, m, sizeof(*mat));
}

void glLoadIdentity(void)
{
	struct gli_matrix_stack *const ms = get_ms(matrix_mode);
	memcpy(ms->mat[ms->top], gli_matrix_id, sizeof(*ms->mat));
}

void glRotatex(GLfixed angle, GLfixed x, GLfixed y, GLfixed z)
{
	angle = Fix_mul(angle >> 3, 1456);
	int32_t const c = Fix_cos(angle);
	int32_t const s = Fix_sin(angle);
	int32_t v[3] = { x, y, z };
	Fix_normalize(v);
	GLfixed m[16];
	int32_t const xy_ = Fix_mul( Fix_mul(v[0], v[1]), 0x10000 - c );
	int32_t const yz_ = Fix_mul( Fix_mul(v[1], v[2]), 0x10000 - c );
	int32_t const xz_ = Fix_mul( Fix_mul(v[0], v[2]), 0x10000 - c );
	int32_t const xs = Fix_mul(v[0], s);
	int32_t const ys = Fix_mul(v[1], s);
	int32_t const zs = Fix_mul(v[2], s);
	m[0] = Fix_mul( Fix_mul(v[0], v[0]), 0x10000 - c ) + c;
	m[1] = xy_ + zs;
	m[2] = xz_ - ys;
	m[3] = 0;
	m[4] = xy_ - zs;
	m[5] = Fix_mul( Fix_mul(v[1], v[1]), 0x10000 - c ) + c;
	m[6] = yz_ + xs;
	m[7] = 0;
	m[8] = xz_ + ys;
	m[9] = yz_ - xs;
	m[10] = Fix_mul( Fix_mul(v[2], v[2]), 0x10000 - c ) + c;
	m[11] = 0;
	m[12] = 0;
	m[13] = 0;
	m[14] = 0;
	m[15] = 0x10000;
	mult_matrix(m);
}

void glMultMatrixx(GLfixed const *m)
{
	mult_matrix(m);
}

void glScalex(GLfixed x, GLfixed y, GLfixed z)
{
	struct gli_matrix_stack *const ms = get_ms(matrix_mode);
	GLfixed (*const mat)[16] = ms->mat + ms->top;
	scale_vec(mat[0], x);
	scale_vec(mat[4], y);
	scale_vec(mat[8], z);
}

void glTranslatex(GLfixed x, GLfixed y, GLfixed z)
{
	GLfixed mat[16];
	memcpy(mat, gli_matrix_id, sizeof(mat));
	mat[12] = x;
	mat[13] = y;
	mat[14] = z;
	mult_matrix(mat);
}

void glFrustumx(GLfixed left, GLfixed right, GLfixed bottom, GLfixed top, GLfixed near, GLfixed far)
{
	if (near <= 0 || far <= 0 || left == right || top == bottom) {
		gli_set_error(GL_INVALID_VALUE);
		return;
	}
	frustum_ortho(1, left, right, bottom, top, near, far);
}

void glOrthox(GLfixed left, GLfixed right, GLfixed bottom, GLfixed top, GLfixed near, GLfixed far)
{
	frustum_ortho(0, left, right, bottom, top, near, far);
}

void glDepthRangex(GLclampx near, GLclampx far)
{
	CLAMP(near, 0, 0x10000);
	CLAMP(far, 0, 0x10000);
	gli_depth_range_near = near;
	gli_depth_range_far = far;
}

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	if (width < 0 || height < 0) {
		gli_set_error(GL_INVALID_VALUE);
		return;
	}
	if (width >= SCREEN_WIDTH) width = SCREEN_WIDTH;
	if (height >= SCREEN_HEIGHT) height = SCREEN_HEIGHT;
	gli_viewport_x = x;
	gli_viewport_y = y;
	gli_viewport_width = width;
	gli_viewport_height = height;
}

