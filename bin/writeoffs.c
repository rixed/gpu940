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
#include <stdio.h>
#include <stddef.h>
#include "gpu940i.h"

#define printctxoff(x) printf("OFF_ctx."#x"=%d\n", offsetof(struct ctx, x))
#define printshroff(x) printf("OFF_shared."#x"=%d\n", offsetof(struct gpuShared, x))

int main(void) {
	printctxoff(view.clipMin);
	printctxoff(view.clipMax);
	printctxoff(view.winPos);
	printctxoff(view.clipPlanes);
	printctxoff(view.nb_clipPlanes);
	printctxoff(view.dproj);
	printctxoff(poly.cmdFacet);
	printctxoff(poly.cmdFacet.size);
	printctxoff(poly.cmdFacet.color);
	printctxoff(poly.vectors);
	printctxoff(poly.z_alpha);
	printctxoff(poly.scan_dir);
	printctxoff(poly.nc_dir);
	printctxoff(poly.first_vector);
	printctxoff(poly.nb_params);
	printctxoff(poly.nc_log);
	printctxoff(location.txt.address);
	printctxoff(location.txt_mask);
	printctxoff(line.count);
	printctxoff(line.w);
	printctxoff(line.dw);
	printctxoff(line.decliv);
	printctxoff(line.ddecliv);
	printctxoff(line.param);
	printctxoff(line.dparam);
	printshroff(buffers);
	printf("BUFFERS_ADDR=0x%x\n", (unsigned)(((struct gpuShared *)(SHARED_PHYSICAL_ADDR-0x2000000U))->buffers));
	return 0;
}
