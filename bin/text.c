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
#include "gpu940.h"
#include "text.h"

/*
 * Data Definitions
 */

// TODO: other arrays for mipmaps half, quarter, etc... and a function
// of libgpu to compute these mipmaps from textures after one get modified.
// (this is not done on the ARM940 but on the ARM920)

/*
 * Private Functions
 */

static uint16_t texture_color(struct buffer_loc const *loc, int32_t u, int32_t v);

/*
 * Public Functions
 */


