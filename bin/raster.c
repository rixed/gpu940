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
	uint32_t *restrict w = ctx.line.w;
	int32_t decliv = ctx.line.decliv;
	int32_t param[GPU_NB_PARAMS];	// (u,v,i)|(r,g,b),z;
	for (unsigned i=sizeof_array(param); i--; ) {
		param[i] = ctx.line.param[i];
	}
	int count = ctx.line.count;
	do {
		uint32_t *w_;
		if (ctx.rendering.mode.named.perspective) {
			w_ = w + ((decliv>>16)<<ctx.poly.nc_log);	
		} else {
			w_ = w;
		}
		// ZBuffer
		if (ctx.rendering.mode.named.z_mode != gpu_z_off) {
			int32_t const zb = *(w_ + ctx.code.out2zb);
			if (! zpass(param[3], ctx.rendering.mode.named.z_mode, zb)) goto next_pixel;
		}
		// Peek color
		uint32_t color;
		switch (ctx.rendering.mode.named.rendering_type) {
			case rendering_flat:
				color = ctx.poly.cmd->color;
				break;
			case rendering_text:
				color = texture_color(&ctx.location.buffer_loc[gpuTxtBuffer], param[0], param[1]);
				if (ctx.rendering.mode.named.use_key) {
					if (color == ctx.poly.cmd->color) goto next_pixel;
				}
				break;
			case rendering_smooth:
				color =
#				ifdef GP2X
					((param[2]&0xFF00)<<16)|((param[0]&0xFF00)<<8)|(param[1]&0xFF00)|((param[0]&0xFF00)>>8);
#				else
					((param[0]&0xFF00)<<8)|(param[1]&0xFF00)|((param[2]&0xFF00)>>8);
#				endif
				break;
			default:
				assert(0);
		}
		// Intens
		if (ctx.rendering.mode.named.use_intens) {
#		ifdef GP2X	// gp2x uses YUV
			int y = color&0xff;
			y += (param[2]>>22);
			SAT8(y);
			color = (color&0xff00ff00) | (y<<16) | y;
#		else
			int r = (color>>16)+(param[2]>>16);
			int g = ((color>>8)&255)+(param[2]>>16);
			int b = (color&255)+(param[2]>>16);
			SAT8(r);
			SAT8(g);
			SAT8(b);
			color = (r<<16)|(g<<8)|b;
#		endif
		}
		// Blend
		if (ctx.rendering.mode.named.blend_coef) {
			uint32_t previous = *w_;
			uint32_t p = (previous&0x00ff00ff)>>(ctx.rendering.mode.named.blend_coef<=8 ? 8-ctx.rendering.mode.named.blend_coef : 0);
			uint32_t c = (color&0x00ff00ff)>>(ctx.rendering.mode.named.blend_coef>8 ? ctx.rendering.mode.named.blend_coef-8 : 0);
			uint32_t merge = ((p>>1) + (c>>1)) & 0x00ff00ff;
			p = (previous&0xff00ff00)>>(ctx.rendering.mode.named.blend_coef<=8 ? 8-ctx.rendering.mode.named.blend_coef : 0);
			c = (color&0xff00ff00)>>(ctx.rendering.mode.named.blend_coef>8 ? ctx.rendering.mode.named.blend_coef-8 : 0);
			color = merge | (((p>>1) + (c>>1)) & 0xff00ff00);
		}
		// Poke
		if (ctx.rendering.mode.named.write_out) {
			*w_ = color;
		}
		if (ctx.rendering.mode.named.write_z) {
			*(w_ + ctx.code.out2zb) = param[3];
		}
		// Next pixel
next_pixel:
		for (unsigned i=sizeof_array(param); i--; ) {
			param[i] += ctx.line.dparam[i];
		}
		if (ctx.rendering.mode.named.perspective) {
			w += ctx.line.dw;
			decliv += ctx.poly.decliveness;
		} else {
			w ++;
		}
	} while (--count >= 0);
}

