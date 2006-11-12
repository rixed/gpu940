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
//	GLboolean enabled;	useless ?
	enum gli_TexEnvMode env_mode;
	GLfixed env_color[4];
	GLuint bound;
	struct matrix_stack ms;
	GLfixed s, t, r, q;
};

struct gli_texture_object {
	// target is always GL_TEXTURE_2D
	// State :
	// - mipmap array (with width, height, border width, internal format, resolutions for r,g,b,a,lum and intens components,
	//                 flag for compression, size of compressed image)
	struct {
		unsigned height_log, width_log;	// this is inconsistant : img_datas should go there, even if is_resident remains global
	} mipmaps[16];
	// - property set : minification and magnification filters, wrap mode for s and t, border_color, min and max LOD, base and max
	//                  mipmap array, flag for resident, depth mode, compare mode, compare function, priority.
	enum gli_TexFilter min_filter, max_filter;
	enum gli_TexWrap wrap_s, wrap_t;
	// - another property set, with another priority.
	// - and of course, image data
	// See glSpecs section 3.8.11
	bool is_resident;	// meaningfull only if has_data
	bool has_data;
	bool was_bound;	// to distinguish between to we created due to genTextures but that do not account as texture obj yet for isTexture
	uint32_t *img_nores;	// if has_data and !is_resident
	struct gpuBuf *img_res;	// if has_data and is_resident
};

int gli_texture_begin(void);
void gli_texture_end(void);
struct gli_texture_unit *gli_get_texture_unit(void);
struct gli_texture_object *gli_get_texture_object(void);

#endif
