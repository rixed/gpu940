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
#ifndef CODEGEN_H_061026
#define CODEGEN_H_061026

#define TEST_RASTERIZER

struct jit_cache *jit_prepare_rasterizer(void);
void jit_invalidate(void);
static inline void jit_exec(void)
{
	typedef void (*rasterizer_func)(void);
	rasterizer_func const rasterizer = (rasterizer_func const)ctx.poly.rasterizer->buf;
	rasterizer();
}

#endif
