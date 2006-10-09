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
#ifndef GLI_H_061009
#define GLI_H_061009

#include "GL/gl.h"
#include "gpu940.h"

#define GLI_MAX_TEXTURE_UNITS 32	// at least 1
#define GLI_MAX_MODELVIEW_STACK_DEPTH 64	// at least 16
#define GLI_MAX_PROJECTION_STACK_DEPTH 2	// at least 2
#define GLI_MAX_TEXTURE_STACK_DEPTH 2	// at least 2

#include "state.h"
#include "arrays.h"

#endif