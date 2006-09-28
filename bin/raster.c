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
#include "gpu940i.h"
#include "text.h"

/*
 * Data Definitions
 */

/*
 * Private Functions
 */

#define SAT8(x) do { if ((x)>=256) (x)=255; else if ((x)<0) (x)=0; } while(0)

static void shadow(uint32_t *w, int32_t intens) {
	uint32_t c = *w;
#ifdef GP2X
	int32_t y = (c&0xff)+intens;
	SAT8(y);
	*w = (c&~0xff)|y;
#else
	int32_t r = (c>>16)+intens;
	int32_t g = ((c>>8)&0xff)+intens;
	int32_t b = (c&0xff)+intens;
	SAT8(r);
	SAT8(g);
	SAT8(b);
	*w = (r<<16)|(g<<8)|b;
#endif
}

/*
 * Public Functions
 */

#ifndef GP2X
void draw_line_c(void) {
	do {
		uint32_t *w = (uint32_t *)(ctx.line.w);
//		if (start_poly) *w = ctx.poly.scan_dir ? 0x3e0 : 0xf800; else
		*w = ctx.poly.cmdFacet.color;
		ctx.line.w += ctx.line.dw;
		ctx.line.count --;
	} while (ctx.line.count >= 0);
}
#endif

void draw_line_shadow(void) {	// used for shadowing a flat polygon
	do {
		uint32_t *w = (uint32_t *)(ctx.line.w);
		shadow(w, ctx.poly.cmdFacet.intens);
		ctx.line.w += ctx.line.dw;
		ctx.line.count --;
	} while (ctx.line.count >= 0);
}

#ifndef GP2X
void draw_line_ci(void) {
	uint32_t const color = ctx.poly.cmdFacet.color;
//	if (start_poly) color |= ctx.poly.scan_dir ? 0x3e0 : 0xf800;
#ifdef GP2X	// gp2x uses YUV
	ctx.line.param[0] *= 55;
	ctx.line.dparam[0] *= 55;
#endif
	do {
		uint32_t *w = (uint32_t *)(ctx.line.w + ((ctx.line.decliv>>16)<<ctx.poly.nc_log));
#ifdef GP2X	// gp2x uses YUV
		int32_t i = ctx.line.param[0]>>22;
		int y = color&0xff;
		y += i;
		SAT8(y);
		*w = (color&0xff00ff00) | (y<<16) | y;
#else
		int32_t i = ctx.line.param[0]>>16;
		int r = (color>>16)+i;
		int g = ((color>>8)&255)+i;
		int b = (color&255)+i;
		SAT8(r);
		SAT8(g);
		SAT8(b);
		*w = (r<<16)|(g<<8)|b;
#endif
		ctx.line.w += ctx.line.dw;
		ctx.line.decliv += ctx.line.ddecliv;
		ctx.line.param[0] += ctx.line.dparam[0];
		ctx.line.count --;
	} while (ctx.line.count >= 0);
}
#endif

#ifndef GP2X
void draw_line_uv(void) {
	do {
//		uint8_t z = ctx.poly.z_alpha>>8;
//		uint32_t color = (z<<16)|(z<<8)|z;
		uint32_t color = texture_color(&ctx.location.txt, ctx.line.param[0], ctx.line.param[1]);
//		if (start_poly) color = ctx.poly.scan_dir ? 0x3e0 : 0xf800;
		uint32_t *w = (uint32_t *)(ctx.line.w + ((ctx.line.decliv>>16)<<ctx.poly.nc_log));
		*w = color;
		ctx.line.w += ctx.line.dw;
		ctx.line.decliv += ctx.line.ddecliv;
		ctx.line.param[0] += ctx.line.dparam[0];
		ctx.line.param[1] += ctx.line.dparam[1];
		ctx.line.count --;
	} while (ctx.line.count >= 0);
}
#endif

void draw_line_uvk(void) {
	do {
		uint32_t color = texture_color(&ctx.location.txt, ctx.line.param[0], ctx.line.param[1]);
		uint32_t *w = (uint32_t *)(ctx.line.w + ((ctx.line.decliv>>16)<<ctx.poly.nc_log));
		if (color != ctx.poly.cmdFacet.color) *w = color;
		ctx.line.w += ctx.line.dw;
		ctx.line.decliv += ctx.line.ddecliv;
		ctx.line.param[0] += ctx.line.dparam[0];
		ctx.line.param[1] += ctx.line.dparam[1];
		ctx.line.count --;
	} while (ctx.line.count >= 0);
}

void draw_line_uvk_shadow(void) {	// used to shadow a keyed uv poly (read texture for keycolor)
	do {
		uint32_t color = texture_color(&ctx.location.txt, ctx.line.param[0], ctx.line.param[1]);
		uint32_t *w = (uint32_t *)(ctx.line.w + ((ctx.line.decliv>>16)<<ctx.poly.nc_log));
		if (color != ctx.poly.cmdFacet.color) shadow(w, ctx.poly.cmdFacet.intens);
		ctx.line.w += ctx.line.dw;
		ctx.line.decliv += ctx.line.ddecliv;
		ctx.line.param[0] += ctx.line.dparam[0];
		ctx.line.param[1] += ctx.line.dparam[1];
		ctx.line.count --;
	} while (ctx.line.count >= 0);
}

#ifndef GP2X
void draw_line_uvi(void) {
#ifdef GP2X
	ctx.line.param[2] *= 55;
	ctx.line.dparam[2] *= 55;
#endif
	do {
		uint32_t color = texture_color(&ctx.location.txt, ctx.line.param[0], ctx.line.param[1]);
//		if (start_poly) color = ctx.poly.scan_dir ? 0x3e0 : 0xf800;
		uint32_t *w = (uint32_t *)(ctx.line.w + ((ctx.line.decliv>>16)<<ctx.poly.nc_log));
		int32_t i = ctx.line.param[2]>>16;
#ifdef GP2X	// gp2x uses YUV
		int y = color&0xff;
		y += i>>6; //(220*i)>>8;
		SAT8(y);
		*w = (color&0xff00ff00) | (y<<16) | y;
#else
		int r = (color>>16)+i;
		int g = ((color>>8)&255)+i;
		int b = (color&255)+i;
		SAT8(r);
		SAT8(g);
		SAT8(b);
		*w = (r<<16)|(g<<8)|b;
#endif
		ctx.line.w += ctx.line.dw;
		ctx.line.decliv += ctx.line.ddecliv;
		ctx.line.param[0] += ctx.line.dparam[0];
		ctx.line.param[1] += ctx.line.dparam[1];
		ctx.line.param[2] += ctx.line.dparam[2];
		ctx.line.count --;
	} while (ctx.line.count >= 0);
}
#endif

