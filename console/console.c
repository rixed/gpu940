#include "console.h"
#include "gpu940.h"
#ifndef GP2X
#	include <assert.h>
#else
#	define assert(x)
#endif

/*
 * Private Datas
 */

#include "font.c"
#ifdef GP2X
#else
SDL_Surface *sdl_console;
#endif

uint16_t color = 0xffff;

/*
 * Private Functions
 */

static inline unsigned char_width(void) { return 8; }
static inline unsigned char_height(void) { return sizeof(consolefont[0]); }

void plot_char(int x, int y, int c) {
	uint16_t *w = (uint16_t *)sdl_console->pixels;
	w += SCREEN_WIDTH*y*char_height() + x*char_width();
	for (unsigned dy=0; dy<char_height(); dy++) {
		for (unsigned dx=0; dx<char_width(); dx++) {
			*(w + dx) = consolefont[c][dy]&(0x80>>dx) ? color:0;
		}
		w += SCREEN_WIDTH;
	}
}

void console_clear_rect_(unsigned x, unsigned y, unsigned width, unsigned height) {
	for (unsigned dy=0; dy<height; dy++) {
		for (unsigned dx=0; dx<width; dx++) {
			plot_char(x+dx, y+dy, ' ');
		}
	}
}

/*
 * Public Functions
 */

void console_begin(void) {
	sdl_console = SDL_CreateRGBSurface(SDL_SWSURFACE|SDL_SRCCOLORKEY, SCREEN_WIDTH, SCREEN_HEIGHT, 16, 0xF800, 0x07E0, 0x0014, 0);
	if (! sdl_console) {
		fprintf(stderr, "CreateRGBSurface failed: %s\n", SDL_GetError());
		exit(1);
	}
	if (0 != SDL_SetColorKey(sdl_console, SDL_SRCCOLORKEY, 0x0000)) {
		fprintf(stderr, "SetColorKey failed: %s\n", SDL_GetError());
		exit(1);
	}
}

void console_end(void) {
	SDL_FreeSurface(sdl_console);
}

unsigned console_width(void) {
	return SCREEN_WIDTH/char_width();
}
unsigned console_height(void) {
	return SCREEN_HEIGHT/char_height();
}

void console_setcolor(uint8_t r, uint8_t g, uint8_t b) {
	color = ((r>>3)<<11)|((g>>2)<<5)|(b>>3);
}

void console_write(int x, int y, char const *str) {
	while (x < 0) {
		x += console_width();
		y --;
	}
	for ( ; *str; str ++) {
		if (x >= (int)console_width()) {
			x = 0;
			y ++;
		}
		if (y >= (int)console_height()) break;
		if (y >= 0) plot_char(x, y, *str);
		x ++;
	}
}

extern inline void console_clear(void);
void console_clear_rect(unsigned x, unsigned y, unsigned width, unsigned height) {
	if (x > console_width() || y > console_height()) return;
	console_clear_rect_(x, y, width ? width:console_width(), height ? height:console_height());
}

