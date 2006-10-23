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

extern int start_poly;

/*
 * Private Functions
 */

#define SAT8(x) do { if ((x)>=256) (x)=255; else if ((x)<0) (x)=0; } while(0)

static void shadow(uint32_t *w, int32_t intens)
{
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

static bool zpass(int32_t z, gpuZMode z_mode, int32_t zb)
{
	switch (z_mode) {
		case gpu_z_off:
			return true;
		case gpu_z_lt:
			return z < zb;
		case gpu_z_eq:
			return z == zb;
		case gpu_z_ne:
			return z != zb;
		case gpu_z_lte:
			return z <= zb;
		case gpu_z_gt:
			return z > zb;
		case gpu_z_gte:
			return z >= zb;
	}
	return false;
}

/*
 * Public Functions
 */

void raster_gen(void)
{
	// 'Registers'
	uint32_t *restrict w = ctx.line.w;	// at most 4 registers (2 if not perspective)
	int32_t dw = ctx.line.dw;
	int32_t decliv, ddecliv;
	if (ctx.poly.cmdFacet.perspective) {
		decliv = ctx.line.decliv;
		ddecliv = ctx.line.ddecliv;
	}
	int nbp = 0;
	int32_t u,du,v,dv,r,dr,g,dg,b,db,i,di,z,dz;	// at most 8 registers (7 if perspective)
	switch (ctx.poly.cmdFacet.rendering_type) {
		case rendering_flat:
			break;
		case rendering_shadow:
			if (! ctx.poly.cmdFacet.use_key) break;
		case rendering_text:
			u = ctx.line.param[0];
			du = ctx.line.dparam[0];
			v = ctx.line.param[1];
			dv = ctx.line.dparam[1];
			nbp += 2;
			break;
		case rendering_smooth:
			r = ctx.line.param[0];
			dr = ctx.line.dparam[0];
			g = ctx.line.param[1];
			dg = ctx.line.dparam[1];
			b = ctx.line.param[2];
			db = ctx.line.dparam[2];
			nbp += 3;
			break;
	}
	if (ctx.poly.cmdFacet.use_intens) {
		assert(ctx.poly.cmdFacet.rendering_type != rendering_smooth);	// i was already dissolved into rgb components. We save one register.
		i = ctx.line.param[nbp];
		di = ctx.line.dparam[nbp++];
#		ifdef GP2X	// gp2x uses YUV
		i *= 55;
		di *= 55;
#		endif
	}
	if (ctx.rendering.z_mode != gpu_z_off) {
		z = ctx.line.param[nbp];
		dz = ctx.line.dparam[nbp++];
	}
	int32_t out2zb = ctx.location.buffer_loc[gpuZBuffer].address - ctx.location.buffer_loc[gpuOutBuffer].address;	// in words
	int count = ctx.line.count;
	do {
		uint32_t *w_;
		if (ctx.poly.cmdFacet.perspective) {
			w_ = w + ((decliv>>16)<<ctx.poly.nc_log);	
		} else {
			w_ = w;
		}
		// ZBuffer
		if (ctx.rendering.z_mode != gpu_z_off) {
			int32_t zb = *(w_+out2zb);
			if (! zpass(z, ctx.rendering.z_mode, zb)) goto next_pixel;
		}
		// Peek color
		uint32_t color;
		switch (ctx.poly.cmdFacet.rendering_type) {
			case rendering_flat:
				color = ctx.poly.cmdFacet.color;
				break;
			case rendering_shadow:
				if (! ctx.poly.cmdFacet.use_key) break;
			case rendering_text:
				color = texture_color(&ctx.location.buffer_loc[gpuTxtBuffer], u, v);
				u += du;
				v += dv;;
				if (ctx.poly.cmdFacet.use_key) {
					if (color == ctx.poly.cmdFacet.color) goto next_pixel;
				}
				break;
			case rendering_smooth:
				color =
#				ifdef GP2X
					((b&0xFF00)<<16)|((r&0xFF00)<<8)|(g&0xFF00)|((r&0xFF00)>>8);
#				else
					((r&0xFF00)<<8)|(g&0xFF00)|((b&0xFF00)>>8);
#				endif
				r += dr;
				g += dg;
				b += db;
				break;
		}
		//if (start_poly) color = ctx.poly.scan_dir ? 0x3e0 : 0xf800;
		// Intens
		if (ctx.poly.cmdFacet.use_intens) {
#		ifdef GP2X	// gp2x uses YUV
			int y = color&0xff;
			y += (i>>22);
			SAT8(y);
			color = (color&0xff00ff00) | (y<<16) | y;
#		else
			int r = (color>>16)+(i>>16);
			int g = ((color>>8)&255)+(i>>16);
			int b = (color&255)+(i>>16);
			SAT8(r);
			SAT8(g);
			SAT8(b);
			color = (r<<16)|(g<<8)|b;
#		endif
			i += di;
		}
		// Shadow
		if (ctx.poly.cmdFacet.rendering_type == rendering_shadow) {
			uint32_t c = *w_;
#		ifdef GP2X
			int32_t y = (c&0xff)+ctx.poly.cmdFacet.shadow;
			SAT8(y);
			color = (c&~0xff)|y;
#		else
			int32_t r = (c>>16)+ctx.poly.cmdFacet.shadow;
			int32_t g = ((c>>8)&0xff)+ctx.poly.cmdFacet.shadow;
			int32_t b = (c&0xff)+ctx.poly.cmdFacet.shadow;
			SAT8(r);
			SAT8(g);
			SAT8(b);
			color = (r<<16)|(g<<8)|b;
#endif
		}
		// Poke
		if (ctx.poly.cmdFacet.write_out) {
			*w_ = color;
		}
		if (ctx.poly.cmdFacet.write_z) {
			*(w_+out2zb) = z;
		}
		// Next pixel
next_pixel:
		if (ctx.poly.cmdFacet.perspective) {
			w += dw;
			decliv += ddecliv;
		} else {
			w ++;
			if (ctx.rendering.z_mode != gpu_z_off) {
				z += dz;
			}
		}
	} while (--count >= 0);
}

#ifndef GP2X
void draw_line_c(void) {
	uint32_t *restrict w = ctx.line.w;
	int count = ctx.line.count;
	do {
//		if (start_poly) *w = ctx.poly.scan_dir ? 0x3e0 : 0xf800; else
		*w = ctx.poly.cmdFacet.color;
		w = (uint32_t *)((uint8_t *)w + ctx.line.dw);
	} while (--count >= 0);
}
#endif

void draw_line_shadow(void) {	// used for shadowing a flat polygon
	uint32_t *restrict w = ctx.line.w;
	int count = ctx.line.count;
	do {
		shadow(w, ctx.poly.cmdFacet.shadow);
		w = (uint32_t *)((uint8_t *)w + ctx.line.dw);
	} while (--count >= 0);
}

#ifndef GP2X
void draw_line_ci(void) {
	int count = ctx.line.count;
	uint32_t const color = ctx.poly.cmdFacet.color;
//	if (start_poly) color |= ctx.poly.scan_dir ? 0x3e0 : 0xf800;
#ifdef GP2X	// gp2x uses YUV
	ctx.line.param[0] *= 55;
	ctx.line.dparam[0] *= 55;
#endif
	do {
		uint32_t *w = (uint32_t *)((uint8_t *)ctx.line.w + ((ctx.line.decliv>>16)<<ctx.poly.nc_log));
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
		ctx.line.w = (uint32_t *)((uint8_t *)ctx.line.w + ctx.line.dw);
		ctx.line.decliv += ctx.line.ddecliv;
		ctx.line.param[0] += ctx.line.dparam[0];
	} while (--count >= 0);
}
#endif

#ifndef GP2X
void draw_line_uv(void) {
	do {
//		uint8_t z = ctx.poly.z_alpha>>8;
//		uint32_t color = (z<<16)|(z<<8)|z;
		uint32_t color = texture_color(&ctx.location.buffer_loc[gpuTxtBuffer], ctx.line.param[0], ctx.line.param[1]);
//		if (start_poly) color = ctx.poly.scan_dir ? 0x3e0 : 0xf800;
		uint32_t *w = (uint32_t *)((uint8_t *)ctx.line.w + ((ctx.line.decliv>>16)<<ctx.poly.nc_log));
		*w = color;
		ctx.line.w = (uint32_t *)((uint8_t *)ctx.line.w + ctx.line.dw);
		ctx.line.decliv += ctx.line.ddecliv;
		ctx.line.param[0] += ctx.line.dparam[0];
		ctx.line.param[1] += ctx.line.dparam[1];
		ctx.line.count --;
	} while (ctx.line.count >= 0);
}
#endif

void draw_line_uvk(void) {
	do {
		uint32_t color = texture_color(&ctx.location.buffer_loc[gpuTxtBuffer], ctx.line.param[0], ctx.line.param[1]);
		uint32_t *w = (uint32_t *)((uint8_t *)ctx.line.w + ((ctx.line.decliv>>16)<<ctx.poly.nc_log));
		if (color != ctx.poly.cmdFacet.color) *w = color;
		ctx.line.w = (uint32_t *)((uint8_t *)ctx.line.w + ctx.line.dw);
		ctx.line.decliv += ctx.line.ddecliv;
		ctx.line.param[0] += ctx.line.dparam[0];
		ctx.line.param[1] += ctx.line.dparam[1];
		ctx.line.count --;
	} while (ctx.line.count >= 0);
}

void draw_line_cs(void) {
	do {
		uint32_t color =
#ifdef GP2X
			((ctx.line.param[2]&0xFF00)<<16)|((ctx.line.param[0]&0xFF00)<<8)|(ctx.line.param[1]&0xFF00)/*|((ctx.line.param[0]&0xFF00)>>8)*/;
#else
			((ctx.line.param[0]&0xFF00)<<8)|(ctx.line.param[1]&0xFF00)|((ctx.line.param[2]&0xFF00)>>8);
#endif
//		if (start_poly) color = ctx.poly.scan_dir ? 0x3e0 : 0xf800;
		uint32_t *w = (uint32_t *)((uint8_t *)ctx.line.w + ((ctx.line.decliv>>16)<<ctx.poly.nc_log));
		*w = color;
		ctx.line.w = (uint32_t *)((uint8_t *)ctx.line.w + ctx.line.dw);
		ctx.line.decliv += ctx.line.ddecliv;
		ctx.line.param[0] += ctx.line.dparam[0];
		ctx.line.param[1] += ctx.line.dparam[1];
		ctx.line.param[2] += ctx.line.dparam[2];
		ctx.line.count --;
	} while (ctx.line.count >= 0);
}

void draw_line_uvk_shadow(void) {	// used to shadow a keyed uv poly (read texture for keycolor)
	do {
		uint32_t color = texture_color(&ctx.location.buffer_loc[gpuTxtBuffer], ctx.line.param[0], ctx.line.param[1]);
		uint32_t *w = (uint32_t *)((uint8_t *)ctx.line.w + ((ctx.line.decliv>>16)<<ctx.poly.nc_log));
		if (color != ctx.poly.cmdFacet.color) shadow(w, ctx.poly.cmdFacet.shadow);
		ctx.line.w = (uint32_t *)((uint8_t *)ctx.line.w + ctx.line.dw);
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
		uint32_t color = texture_color(&ctx.location.buffer_loc[gpuTxtBuffer], ctx.line.param[0], ctx.line.param[1]);
//		if (start_poly) color = ctx.poly.scan_dir ? 0x3e0 : 0xf800;
		uint32_t *w = (uint32_t *)((uint8_t *)ctx.line.w + ((ctx.line.decliv>>16)<<ctx.poly.nc_log));
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
		ctx.line.w = (uint32_t *)((uint8_t *)ctx.line.w + ctx.line.dw);
		ctx.line.decliv += ctx.line.ddecliv;
		ctx.line.param[0] += ctx.line.dparam[0];
		ctx.line.param[1] += ctx.line.dparam[1];
		ctx.line.param[2] += ctx.line.dparam[2];
		ctx.line.count --;
	} while (ctx.line.count >= 0);
}
#endif

#ifndef GP2X
void draw_line_uvi_lin(void) {
#ifdef GP2X
	ctx.line.param[2] *= 55;
	ctx.line.dparam[2] *= 55;
#endif
	do {
		uint32_t color = texture_color(&ctx.location.buffer_loc[gpuTxtBuffer], ctx.line.param[0], ctx.line.param[1]);
//		if (start_poly) color = ctx.poly.scan_dir ? 0x3e0 : 0xf800;
		uint32_t *w = (uint32_t *)(ctx.line.w);
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
		ctx.line.w ++;
		ctx.line.param[0] += ctx.line.dparam[0];
		ctx.line.param[1] += ctx.line.dparam[1];
		ctx.line.param[2] += ctx.line.dparam[2];
		ctx.line.count --;
	} while (ctx.line.count >= 0);
}
#endif
