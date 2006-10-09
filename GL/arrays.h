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
#ifndef ARRAYS_H_061009
#define ARRAYS_H_061009

extern struct array {
	const GLvoid *ptr;
	GLsizei stride;
	enum gli_Types type;
	GLint size;
	unsigned enabled:1;
};

int gli_arrays_begin(void);
static inline void gli_arrays_end(void) {}

// get_veretxes(), etc...

#endif
