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

/*
 * Data Definitions
 */

/*
 * Private Functions
 */

#define SAT8(x) do { if ((x)>=256) (x)=255; else if ((x)<0) (x)=0; } while(0)

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
	int32_t decliv = ctx.line.decliv;
	int nbp = 0;
	int32_t u,v,r,g,b,i,di,z,dz;	// at most 8 registers (7 if perspective)
	switch (ctx.poly.cmdFacet.rendering_type) {
		case rendering_flat:
			break;
		case rendering_text:
			u = ctx.line.param[0];
			v = ctx.line.param[1];
			nbp += 2;
			break;
		case rendering_smooth:
			r = ctx.line.param[0];
			g = ctx.line.param[1];
			b = ctx.line.param[2];
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
			int32_t zb = *(w_ + ctx.code.out2zb);
			if (! zpass(z, ctx.rendering.z_mode, zb)) goto next_pixel;
		}
		// Peek color
		uint32_t color;
		switch (ctx.poly.cmdFacet.rendering_type) {
			case rendering_flat:
				color = ctx.poly.cmdFacet.color;
				break;
			case rendering_text:
				color = texture_color(&ctx.location.buffer_loc[gpuTxtBuffer], u, v);
				u += ctx.line.dparam[0];
				v += ctx.line.dparam[1];
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
				r += ctx.line.dparam[0];
				g += ctx.line.dparam[1];
				b += ctx.line.dparam[2];
				break;
		}
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
		// Blend
		if (ctx.poly.cmdFacet.blend_coef) {
			uint32_t previous = *w_;
			uint32_t p = (previous&0x00ff00ff)>>(ctx.poly.cmdFacet.blend_coef<=8 ? 8-ctx.poly.cmdFacet.blend_coef : 0);
			uint32_t c = (color&0x00ff00ff)>>(ctx.poly.cmdFacet.blend_coef>8 ? ctx.poly.cmdFacet.blend_coef-8 : 0);
			uint32_t merge = ((p + c) >> 1) & 0x00ff00ff;
			p = (previous&0xff00ff00)>>(ctx.poly.cmdFacet.blend_coef<=8 ? 8-ctx.poly.cmdFacet.blend_coef : 0);
			c = (color&0xff00ff00)>>(ctx.poly.cmdFacet.blend_coef>8 ? ctx.poly.cmdFacet.blend_coef-8 : 0);
			color = merge | (((p + c) >> 1) & 0xff00ff00);
		}
		// Poke
		if (ctx.poly.cmdFacet.write_out) {
			*w_ = color;
		}
		if (ctx.poly.cmdFacet.write_z) {
			*(w_ + ctx.code.out2zb) = z;
		}
		// Next pixel
next_pixel:
		if (ctx.poly.cmdFacet.perspective) {
			w += ctx.line.dw;
			decliv += ctx.poly.decliveness;
		} else {
			w ++;
			if (ctx.rendering.z_mode != gpu_z_off) {
				z += dz;
			}
		}
	} while (--count >= 0);
}

