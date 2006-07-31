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
#ifndef CONSOLE_H_060518
#define CONSOLE_H_060518

#include <stdint.h>

#ifndef GP2X
#	include <SDL/SDL.h>
extern SDL_Surface *sdl_console;
#endif

void console_begin(void);
void console_end(void);
void console_enable(void);

unsigned console_width(void);
unsigned console_height(void);

void console_setcolor(uint8_t c);
void console_write(int x, int y, char const *str);
void console_write_uint(int x, int y, unsigned nb_digits, uint32_t number);
void console_clear_rect(unsigned x, unsigned y, unsigned width, unsigned height); // if width=0, use console_width ; idem for height.
static inline void console_clear(void) { console_clear_rect(0, 0, 0, 0); }

#endif
