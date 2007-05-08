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
/* This handle key inputs both for X11 and GP2X.
 */
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include "gpu940.h"
#include "input.h"

/*
 * Data Definitions
 */

/*
 * Private Functions
 */

#ifndef GP2X
#include <X11/Xlib.h>
#include <X11/Xatom.h>
/* X11 : we need some functions to grab gpu940's window inputs */
static Atom wm_name_atom;
static Atom string_atom;
static Display *display;
static struct {
	char name[8];
	int keycode;
	enum gpuInput on_press, on_release;
} x11_keys[] = {
	{ "Click",   65, GPU_PRESS_CLICK,  GPU_RELEASE_CLICK },
	{ "A",       38, GPU_PRESS_A,      GPU_RELEASE_A },
	{ "B",       56, GPU_PRESS_B,      GPU_RELEASE_B },
	{ "X",       53, GPU_PRESS_X,      GPU_RELEASE_X },
	{ "Y",       29, GPU_PRESS_Y,      GPU_RELEASE_Y },
	{ "Start",   28, GPU_PRESS_START,  GPU_RELEASE_START },
	{ "Select",  26, GPU_PRESS_SELECT, GPU_RELEASE_SELECT },
	{ "Left",    46, GPU_PRESS_LEFT,   GPU_RELEASE_LEFT },
	{ "Right",   27, GPU_PRESS_RIGHT,  GPU_RELEASE_RIGHT },
	{ "+Volume", 21, GPU_PRESS_VOL_HI, GPU_RELEASE_VOL_HI },
	{ "-Volume", 20, GPU_PRESS_VOL_LO, GPU_RELEASE_VOL_LO },
};

static struct {
	char name[8];
	int keycode;
	bool pressed;
} joy_pos[] = {
	{ "UP",     98, false },
	{ "Down",  104, false },
	{ "Left",  100, false },
	{ "Right", 102, false },
};

static bool look_for_gpu940_window_recurs(Window current, Window *result)
{
	// Is the current window the one we are looking for ?
	Atom actual_type;
	int actual_format;
	unsigned long len = 0, left;
	unsigned char *prop = NULL;
	bool found;
	if (Success != XGetWindowProperty(display, current, wm_name_atom, 0, 4, False, string_atom, &actual_type, &actual_format, &len, &left, &prop)) {
		abort();
	}
	if (prop) {
		found = 0 == strcmp((char *)prop, "gpu940");
		XFree(prop);
		if (found) {
			*result = current;
			return true;
		}
	}
	// Lets try the children
	Window root;
	Window parent;
	Window *children;
	unsigned nb_children;
	if (0 == XQueryTree(display, current, &root, &parent, &children, &nb_children)) {
		abort();
	}
	found = false;
	while (!found && nb_children--) {
		found = look_for_gpu940_window_recurs(children[nb_children], result);
	}
	if (children) XFree(children);
	return found;
}

static bool look_for_gpu940_window(Window *result)
{
	wm_name_atom = XInternAtom(display, "WM_NAME", True);
	string_atom = XInternAtom(display, "STRING", True);
	return look_for_gpu940_window_recurs(DefaultRootWindow(display), result);
}

static int x11_begin(void)
{
	display = XOpenDisplay(NULL);
	if (! display) {
		fprintf(stderr, "Cannot open X11 display\n");
		goto err0;
	}
	Window gpu_win;
	if (! look_for_gpu940_window(&gpu_win)) {
		fprintf(stderr, "Cannot find gpu940 window\n");
		goto err1;
	}
/*	int ret = XGrabKeyboard(display, gpu_win, False, GrabModeAsync, GrabModeSync, CurrentTime);
	if (Success != ret) {
		fprintf(stderr, "Cannot grab x11 key : %d\n", ret);
		goto err2;
	}*/
	XAutoRepeatOff(display);
	XSelectInput(display, gpu_win, KeyPressMask|KeyReleaseMask/*FocusChangeMask*/);
	return 0;
err2:
err1:
	XCloseDisplay(display);
err0:
	return -1;
}

static void x11_end(void)
{
//	XUngrabKeyboard(display, CurrentTime);
	XAutoRepeatOn(display);
	XCloseDisplay(display);
}

static enum gpuInput joy_event(void)
{
	unsigned state = 0;
	for (unsigned k=0; k<sizeof_array(joy_pos); k++) {
		state = (state << 1) | joy_pos[k].pressed;
	}
	// UP|DW|LT|RG
#	define RG 1
#	define LT 2
#	define DW 4
#	define UP 8
	switch (state) {
		case 0:     return GPU_JOY_CENTER;
		case RG:    return GPU_JOY_RIGHT;
		case LT:    return GPU_JOY_LEFT;
		case DW:    return GPU_JOY_DOWN;
		case UP:    return GPU_JOY_UP;
		case UP+RG: return GPU_JOY_UP_RIGHT;
		case UP+LT: return GPU_JOY_UP_LEFT;
		case DW+RG: return GPU_JOY_DOWN_RIGHT;
		case DW+LT: return GPU_JOY_DOWN_LEFT;
	}
#	undef UP
#	undef DW
#	undef LT
#	undef RG
	fprintf(stderr, "Unreachable JOY state reached !\n");
	return GPU_JOY_CENTER;
}

static enum gpuInput xev2gpuev(int keycode, bool pressed)
{
	// First look if it's a simple key translation
	for (unsigned k=0; k<sizeof_array(x11_keys); k++) {
		if (x11_keys[k].keycode == keycode) {
			return pressed ? x11_keys[k].on_press : x11_keys[k].on_release;
		}
	}
	// Then look for the JOY changing state
	for (unsigned k=0; k<sizeof_array(joy_pos); k++) {
		if (joy_pos[k].keycode == keycode) {
			joy_pos[k].pressed = pressed;
			return joy_event();
		}
	}
}

static enum gpuInput x11_event(void)
{
	if (0 == XPending(display)) return GPU_INPUT_NONE;
	XEvent event;
	XNextEvent(display, &event);
	switch (event.type) {
		case KeyPress:
			return xev2gpuev(event.xkey.keycode, true);
		case KeyRelease:
			return xev2gpuev(event.xkey.keycode, false);
		default:
			return GPU_INPUT_NONE;
	}
}

#endif

static void my_end(void)
{
#	ifndef GP2X
	x11_end();
#	endif
}

/*
 * Public Functions
 */

int gpuInputInit(void)
{
	int ret;
#	ifndef GP2X
	ret = x11_begin();
#	else
	ret = -1;
#	endif
	if (0 != ret) return ret;
	atexit(my_end);
	return 0;
}

enum gpuInput gpuInput(void)
{
#	ifndef GP2X
	return x11_event();
#	else
	return GPU_INPUT_NONE;
#	endif
}

