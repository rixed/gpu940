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
#ifndef GL_TEXTURE_H_061010
#define GL_TEXTURE_H_061010

struct gli_texture_unit {
	GLboolean enabled;
	enum gli_TexEnvMode env_mode;
	GLfixed env_color[4];
	GLuint bound;
	struct matrix_stack ms;
	GLfixed s, t, r, q;
};

int gli_texture_begin(void);
void gli_texture_end(void);
struct gli_texture_unit *gli_get_texture_unit(void);

#endif