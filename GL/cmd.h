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
#ifndef CMD_H_061122
#define CMD_H_061122

static inline int gli_cmd_begin(void) { return 0; }
static inline void gli_cmd_end(void) {}
void gli_cmd_prepare(enum gli_DrawMode mode_);
void gli_cmd_vertex(int32_t const *v);
void gli_facet_array(enum gli_DrawMode mode, GLint first, unsigned count);
void gli_points_array(GLint first, unsigned count);
void gli_clear(gpuBufferType type, GLclampx *val);
static inline uint32_t *gli_get_texture_address(struct gpuBuf *const buf)
{
	return &shared->buffers[gpuBuf_get_loc(buf)->address];
}

#endif
