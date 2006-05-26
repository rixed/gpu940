#include "console.h"
#include "bin/gpu940i.h"
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

uint8_t color;

/*
 * Private Functions
 */

static inline unsigned char_width(void) { return 8; }
static inline unsigned char_height(void) { return sizeof(consolefont[0]); }

void plot_char(int x, int y, int c) {
	if (x < 0 || x >= (int)console_width() || y<0 || y>=(int)console_height()) return;
#ifdef GP2X
	uint8_t *w = shared->osd_data;
	w += SCREEN_WIDTH*y*char_height()/4 + x*char_width()/4;
	for (unsigned dy=0; dy<char_height(); dy++) {
		uint16_t v = 0;	// works because char_width = 8 :-<
		for (unsigned dx=0; dx<char_width(); dx++) {
			if (consolefont[c][dy]&(0x80>>dx)) {
				v |= (uint16_t)color<<(dx*2);
			}
		}
		*w = v&0xff;
		*(w+1) = (v>>8)&0xff;
		w += SCREEN_WIDTH/4;
	}
#else
	uint8_t *w = (uint8_t *)sdl_console->pixels;
	w += SCREEN_WIDTH*y*char_height() + x*char_width();
	for (unsigned dy=0; dy<char_height(); dy++) {
		for (unsigned dx=0; dx<char_width(); dx++) {
			*(w + dx) = consolefont[c][dy]&(0x80>>dx) ? color:0;
		}
		w += SCREEN_WIDTH;
	}
#endif
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
	color = 1;
#ifdef GP2X
	// set address of OSD headers
	uint32_t osd_head = (uint32_t)&((struct gpuShared *)SHARED_PHYSICAL_ADDR)->osd_head;
	gp2x_regs16[0x2916>>1] = osd_head&0xffff;
	gp2x_regs16[0x2918>>1] = osd_head>>16;
	gp2x_regs16[0x291a>>1] = osd_head&0xffff;
	gp2x_regs16[0x291c>>1] = osd_head>>16;
	// set osd palette
	gp2x_regs16[0x2954>>1] = 0;
	gp2x_regs16[0x2956>>1] = 0;	// color 0 (should not be set)
	gp2x_regs16[0x2956>>1] = 0;
	// color 1
	gp2x_regs16[0x2956>>1] = 0x3020;// CbCr
	gp2x_regs16[0x2956>>1] = 0xff;// AlphY
	// color 2
	gp2x_regs16[0x2956>>1] = 0x8080;
	gp2x_regs16[0x2956>>1] = 0x80;	// color 2
	// color 3
	gp2x_regs16[0x2956>>1] = 0xff10;
	gp2x_regs16[0x2956>>1] = 0x90;	// color 3
	// enable OSD
	gp2x_regs16[0x2880>>1] |= 0x80;
#else
	sdl_console = SDL_CreateRGBSurface(SDL_SWSURFACE|SDL_SRCCOLORKEY, SCREEN_WIDTH, SCREEN_HEIGHT, 8, 0, 0, 0, 0);
	if (! sdl_console) {
		fprintf(stderr, "CreateRGBSurface failed: %s\n", SDL_GetError());
		exit(1);
	}
	SDL_Color palette[] = {
		{ 255, 30, 30, 0 },
		{ 30, 255, 30, 0 },
		{ 30, 30, 255, 0 },
	};
	SDL_SetPalette(sdl_console, SDL_LOGPAL, palette, 1, 3);
	if (0 != SDL_SetColorKey(sdl_console, SDL_SRCCOLORKEY, 0x0000)) {
		fprintf(stderr, "SetColorKey failed: %s\n", SDL_GetError());
		exit(1);
	}
#endif
}

void console_end(void) {
#ifdef GP2X
	// disable OSD
	gp2x_regs16[0x2880>>1] &= ~0x80;
#else
	SDL_FreeSurface(sdl_console);
#endif
}

unsigned console_width(void) {
	return SCREEN_WIDTH/char_width();
}
unsigned console_height(void) {
	return SCREEN_HEIGHT/char_height();
}

void console_setcolor(uint8_t c) {
	color =
#ifdef GP2X
		c&3;
#else
		c;
#endif
}

void console_write(int x, int y, char const *str) {
	for ( ; *str; str ++) {
		plot_char(x++, y, *str);
	}
}

void console_write_uint(int x, int y, unsigned nb_digits, uint32_t number) {
	int first = 1;
	while (nb_digits) {
		int c = ' ';
		if (number || first) {
			c = '0' + number%10;
			number /= 10;
			first = 0;
		}
		plot_char(x+(--nb_digits), y, c);
	}
}

extern inline void console_clear(void);
void console_clear_rect(unsigned x, unsigned y, unsigned width, unsigned height) {
	if (x > console_width() || y > console_height()) return;
	console_clear_rect_(x, y, width ? width:console_width(), height ? height:console_height());
}

