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

/*
 * Data Definitions
 */

static bool in_begend;	// weither or not we are between a glBegin/End pair

/*
 * Private Functions
 */

/*
 * Public Functions
 */

int gli_begend_begin(void)
{
	in_begend = false;
	return 0;
}

extern inline void gli_begend_end(void);

void glBegin(GLenum mode)
{
	if (in_begend) {
		return gli_set_error(GL_INVALID_OPERATION);
	}
	if (/*mode < GL_POINTS ||*/ mode > GL_POLYGON) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	gli_cmd_prepare(mode);
	in_begend = true;
}

void glEnd(void)
{
	if (! in_begend) {
		return gli_set_error(GL_INVALID_OPERATION);
	}
	gli_cmd_submit();
	in_begend = false;
}

void glVertex4xv(GLfixed const *v)
{
	assert(in_begend);	// outside of begin/end -> undefined behaviour
	gli_cmd_vertex(v);	// will send the command if the primitive is complete
}
extern inline void glVertex2x(GLfixed x, GLfixed y);
extern inline void glVertex3x(GLfixed x, GLfixed y, GLfixed z);
extern inline void glVertex4x(GLfixed x, GLfixed y, GLfixed z, GLfixed w);
extern inline void glVertex2xv(GLfixed const *v);
extern inline void glVertex3xv(GLfixed const *v);

