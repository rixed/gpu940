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
#include <stdbool.h>

/*
 * Data Definitions
 */

static uint32_t capabilities;
static enum gli_HintMode hints[NB_HINT_TARGETS];

/*
 * Private Functions
 */

#define CAP_BIT(c) (1<<((c)-GL_ALPHA_TEST))

/*
 * Public Functions
 */

int gli_modes_begin(void)
{
	capabilities = CAP_BIT(GL_DITHER) | CAP_BIT(GL_MULTISAMPLE);
	for (unsigned i=0; i<NB_HINT_TARGETS; i++) {
		hints[i] = GL_DONT_CARE;
	}
	return 0;
}

extern inline void gli_modes_end(void);

void glEnable(GLenum cap)
{
	if (cap >= GL_LIGHT0 && cap < GL_LIGHT0 + GLI_MAX_LIGHTS) {
		return gli_light_enable(cap-GL_LIGHT0);
	}
	if (cap < GL_ALPHA_TEST || cap > GL_TEXTURE_2D) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	capabilities |= CAP_BIT(cap);
}

void glDisable(GLenum cap)
{
	if (cap >= GL_LIGHT0 && cap < GL_LIGHT0 + GLI_MAX_LIGHTS) {
		return gli_light_disable(cap-GL_LIGHT0);
	}
	if (cap < GL_ALPHA_TEST || cap > GL_TEXTURE_2D) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	capabilities &= ~CAP_BIT(cap);
}

void glFinish(void)
{
	// TODO wait until no more commands are waiting in the cmd buffer
}

extern inline void glFlush(void);

void glHint(GLenum target, GLenum mode)
{
	if (target >= NB_HINT_TARGETS) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	if (/*mode < GL_FASTEST ||*/ mode > GL_DONT_CARE) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	hints[target] = mode;
}

bool gli_enabled(GLenum cap)
{
	if (cap >= GL_LIGHT0 && cap < GL_LIGHT0 + GLI_MAX_LIGHTS) {
		return gli_light_enabled(cap-GL_LIGHT0);
	}
	return capabilities & CAP_BIT(cap);	
}
