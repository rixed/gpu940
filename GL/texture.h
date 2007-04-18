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

struct gli_texture_object {
	struct gli_mipmap_data {	// Image data
		unsigned height_log, width_log;	// size of this level picture
		bool is_resident;	// meaningfull only if has_data
		bool has_data;
		uint32_t *img_nores;	// if has_data and !is_resident
		struct gpuBuf *img_res;	// if has_data and is_resident
		GLfixed mean_alpha;
		bool need_key, have_mean_alpha;
	} mipmaps[GLI_MAX_TEXTURE_SIZE_LOG+1];
	// These parameters are the same for all mipmaps, although this is unclear if they are required to be
	enum gli_TexFilter min_filter, max_filter;
	enum gli_TexWrap wrap_s, wrap_t;
	bool was_bound;	// to distinguish between TO we created due to genTextures but that do not account as texture obj yet for isTexture
};
// Reserve this color for the key color. I didn't like This blue anyway.
#define KEY_RED 0U
#define KEY_GREEN 0U
#define KEY_BLUE 255U
#define KEY_ALPHA 255U

int gli_texture_begin(void);
void gli_texture_end(void);
struct gli_texture_object *gli_get_texture_object(void);

#endif
