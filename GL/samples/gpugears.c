/*
 * Copyright (C) 1999-2001  Brian Paul   All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/* $XFree86: xc/programs/glxgears/glxgears.c,v 1.3 2001/11/03 17:29:20 dawes Exp $ */

/*
 * This is a port of the now famous "glxgears" demo to gpu940 (i.e. no GLX)
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include "GL/gl.h"
#include "fixmath.h"

/* return current time (in seconds) */
static int current_time(void)
{
	struct timeval tv;
	struct timezone tz;
	(void) gettimeofday(&tv, &tz);
	return (int) tv.tv_sec;
}

static GLfixed view_rotx = 20<<16, view_roty = 30<<16, view_rotz = 0;
static GLfixed angle = 0;

static GLfixed *gear1, *gear2, *gear3;
static unsigned gear1_count, gear2_count, gear3_count;

static void addVertex(GLfixed *arr, unsigned *c, GLfixed x, GLfixed y, GLfixed z) {
	arr[3**c+0] = x;
	arr[3**c+1] = y;
	arr[3**c+2] = z;
	(*c)++;
}
		
/*
 *
 *  Draw a gear wheel.  You'll probably want to call this function when
 *  building a display list since we do a lot of trig here.
 * 
 *  Input:  inner_radius - radius of hole at center
 *          outer_radius - radius at center of teeth
 *          width - width of gear
 *          teeth - number of teeth
 *          tooth_depth - depth of tooth
 */


// TODO : utiliser un seul static tableau pour les points, et mettre le DrawArray dans le gear().
// Ainsi, on peut faire plusieurs DrawArray, utiliser les normales, et tantot des STRIP tantot des TRIANGLE.
static void gear(GLfixed **dest, unsigned *count, GLfixed inner_radius, GLfixed outer_radius, GLfixed width, GLint teeth, GLfixed tooth_depth)
{
	GLint i;
	GLfixed r0, r1, r2;
	GLfixed angle, da;
	GLfixed u, v, len;

	r0 = inner_radius;
	r1 = outer_radius - (tooth_depth >> 1);
	r2 = outer_radius + (tooth_depth >> 1);

	da = (FIXED_PI/teeth) >> 1;

	*count = 0;

//	glNormal3x(0, 0, 1<<16);
	/* draw front face */
	
	for (i = 0; i <= teeth; i++) {
		angle = i * 2 * FIXED_PI / teeth;
		addVertex(*dest, count, Fix_mul(r0, Fix_cos(angle)), Fix_mul(r0, Fix_sin(angle)), width>>1);
		addVertex(*dest, count, Fix_mul(r1, Fix_cos(angle)), Fix_mul(r1, Fix_sin(angle)), width>>1);
		if (i < teeth) {
			addVertex(*dest, count, Fix_mul(r0, Fix_cos(angle)), Fix_mul(r0, Fix_sin(angle)), width>>1);
			addVertex(*dest, count, Fix_mul(r1, Fix_cos(angle + 3 * da)), Fix_mul(r1, Fix_sin(angle + 3 * da)), width>>1);
		}
	}
#if 0
	/* draw front sides of teeth */
	glBegin(GL_QUADS);
	for (i = 0; i < teeth; i++) {
		angle = i * 2 * FIXED_PI / teeth;

		glVertex3x(Fix_mul(r1, Fix_cos(angle)), Fix_mul(r1, Fix_sin(angle)), width>>1);
		glVertex3x(Fix_mul(r2, Fix_cos(angle + da)), Fix_mul(r2, Fix_sin(angle + da)), width>>1);
		glVertex3x(Fix_mul(r2, Fix_cos(angle + 2 * da)), Fix_mul(r2, Fix_sin(angle + 2 * da)), width>>1);
		glVertex3x(Fix_mul(r1, Fix_cos(angle + 3 * da)), Fix_mul(r1, Fix_sin(angle + 3 * da)), width>>1);
	}
	glEnd();
#endif
//	glNormal3x(0, 0, -1<<16);

	/* draw back face */
	for (i = 0; i <= teeth; i++) {
		angle = i * 2 * FIXED_PI / teeth;
		addVertex(*dest, count, Fix_mul(r1, Fix_cos(angle)), Fix_mul(r1, Fix_sin(angle)), -width>>1);
		addVertex(*dest, count, Fix_mul(r0, Fix_cos(angle)), Fix_mul(r0, Fix_sin(angle)), -width>>1);
		if (i < teeth) {
			addVertex(*dest, count, Fix_mul(r1, Fix_cos(angle + 3 * da)), Fix_mul(r1, Fix_sin(angle + 3 * da)), -width>>1);
			addVertex(*dest, count, Fix_mul(r0, Fix_cos(angle)), Fix_mul(r0, Fix_sin(angle)), -width>>1);
		}
	}
#if 0
	/* draw back sides of teeth */
	glBegin(GL_QUADS);
	for (i = 0; i < teeth; i++) {
		angle = i * 2 * FIXED_PI / teeth;

		glVertex3x(Fix_mul(r1, Fix_cos(angle + 3 * da)), Fix_mul(r1, Fix_sin(angle + 3 * da)), -width>>1);
		glVertex3x(Fix_mul(r2, Fix_cos(angle + 2 * da)), Fix_mul(r2, Fix_sin(angle + 2 * da)), -width>>1);
		glVertex3x(Fix_mul(r2, Fix_cos(angle + da)), Fix_mul(r2, Fix_sin(angle + da)), -width>>1);
		glVertex3x(Fix_mul(r1, Fix_cos(angle)), Fix_mul(r1, Fix_sin(angle)), -width>>1);
	}
	glEnd();
#endif

	/* draw outward faces of teeth */
	for (i = 0; i < teeth; i++) {
		angle = i * 2 * FIXED_PI / teeth;
		addVertex(*dest, count, Fix_mul(r1, Fix_cos(angle)), Fix_mul(r1, Fix_sin(angle)), width>>1);
		addVertex(*dest, count, Fix_mul(r1, Fix_cos(angle)), Fix_mul(r1, Fix_sin(angle)), -width>>1);
		u = Fix_mul(r2, Fix_cos(angle + da)) - Fix_mul(r1, Fix_cos(angle));
		v = Fix_mul(r2, Fix_sin(angle + da)) - Fix_mul(r1, Fix_sin(angle));
		len = Fix_sqrt(Fix_mul(u, u) + Fix_mul(v, v));
		u = Fix_div(u, len);
		v = Fix_div(v, len);
		//glNormal3f(v, -u, 0.0);
		addVertex(*dest, count, Fix_mul(r2, Fix_cos(angle + da)), Fix_mul(r2, Fix_sin(angle + da)), width>>1);
		addVertex(*dest, count, Fix_mul(r2, Fix_cos(angle + da)), Fix_mul(r2, Fix_sin(angle + da)), -width>>1);
		//glNormal3f(Fix_cos(angle), Fix_sin(angle), 0.0);
		addVertex(*dest, count, Fix_mul(r2, Fix_cos(angle + 2 * da)), Fix_mul(r2, Fix_sin(angle + 2 * da)), width>>1);
		addVertex(*dest, count, Fix_mul(r2, Fix_cos(angle + 2 * da)), Fix_mul(r2, Fix_sin(angle + 2 * da)), -width>>1);
		u = Fix_mul(r1, Fix_cos(angle + 3 * da)) - Fix_mul(r2, Fix_cos(angle + 2 * da));
		v = Fix_mul(r1, Fix_sin(angle + 3 * da)) - Fix_mul(r2, Fix_sin(angle + 2 * da));
		//glNormal3f(v, -u, 0.0);
		addVertex(*dest, count, Fix_mul(r1, Fix_cos(angle + 3 * da)), Fix_mul(r1, Fix_sin(angle + 3 * da)), width>>1);
		addVertex(*dest, count, Fix_mul(r1, Fix_cos(angle + 3 * da)), Fix_mul(r1, Fix_sin(angle + 3 * da)), -width>>1);
		//glNormal3f(Fix_cos(angle), Fix_sin(angle), 0.0);
	}

	addVertex(*dest, count, Fix_mul(r1, Fix_cos(0)), Fix_mul(r1, Fix_sin(0)), width>>1);
	addVertex(*dest, count, Fix_mul(r1, Fix_cos(0)), Fix_mul(r1, Fix_sin(0)), -width>>1);

	/* draw inside radius cylinder */
	for (i = 0; i <= teeth; i++) {
		angle = i * 2 * FIXED_PI / teeth;
		addVertex(*dest, count, -Fix_cos(angle), -Fix_sin(angle), 0);
		addVertex(*dest, count, Fix_mul(r0, Fix_cos(angle)), Fix_mul(r0, Fix_sin(angle)), -width>>1);
		addVertex(*dest, count, Fix_mul(r0, Fix_cos(angle)), Fix_mul(r0, Fix_sin(angle)), width>>1);
	}
}


static void draw(void)
{
	static GLfixed red[4] = { 52429, 6554, 0, 1<<16 };
	static GLfixed green[4] = { 0, 52429, 13107, 1<<16 };
	static GLfixed blue[4] = { 13107, 13107, 1<<16, 1<<16 };

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPushMatrix();
	glRotatex(view_rotx, 1<<16, 0, 0);
	glRotatex(view_roty, 0, 1<<16, 0);
	glRotatex(view_rotz, 0, 0, 1<<16);

	glPushMatrix();
	glTranslatex(-3<<16, -2<<16, 0);
	glRotatex(angle, 0, 0, 1<<16);
	//glMaterialxv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
	glColor4x(52429, 6554, 0, 1<<16);	// implements lighting
	gear(&gear1, &gear1_count, 1<<16, 4<<16, 1<<16, 20, 45875);
	glVertexPointer(3, GL_FIXED, 0, gear1);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, gear1_count);
	glPopMatrix();

	glPushMatrix();
	glTranslatex(203162, -2<<16, 0);
	glRotatex(Fix_mul(-2<<16, angle) - (9<<16), 0, 0, 1<<16);
	//glMaterialxv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green);
	glColor4x(0, 52429, 13107, 1<<16);
	gear(&gear2, &gear2_count, 1<<15, 2<<16, 2<<16, 10, 45875);
	glVertexPointer(3, GL_FIXED, 0, gear2);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, gear2_count);
	glPopMatrix();

	glPushMatrix();
	glTranslatex(-203162, 275251, 0);
	glRotatex(Fix_mul(-2<<16, angle) - (25<<16), 0, 0, 1<<16);
	//glMaterialxv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
	glColor4x(13107, 13107, 1<<16, 1<<16);
	gear(&gear3, &gear3_count, 85197, 2<<16, 1<<15, 10, 45875);
	glVertexPointer(3, GL_FIXED, 0, gear3);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, gear3_count);
	glPopMatrix();

	glPopMatrix();
}


static void init(void)
{
	static GLfixed pos[4] = { 5<<16, 5<<16, 10<<16, 0 };

	glLightxv(GL_LIGHT0, GL_POSITION, pos);
	glEnable(GL_CULL_FACE);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_NORMALIZE);
	glEnable(GL_VERTEX_ARRAY);
	glShadeModel(GL_FLAT);

	unsigned nb_vec = 1000;
	gear1 = malloc(nb_vec*sizeof(GLfixed));
	assert(gear1);
	gear2 = malloc(nb_vec*sizeof(GLfixed));
	assert(gear2);
	gear3 = malloc(nb_vec*sizeof(GLfixed));
	assert(gear3);
}


/*
 * Create an RGB, double-buffered window.
 * Return the window and context handles.
 */
static void make_window(void)
{
	if (GL_TRUE != glOpen()) {
		printf("Error: couldn't init GPU\n");
		exit(1);
	}
//	GLfloat h = (GLfloat) height / (GLfloat) width;
//	glViewport(0, 0, (GLint) width, (GLint) height);
//	glMatrixMode(GL_PROJECTION);
//	glLoadIdentity();
//	glFrustum(-1.0, 1.0, -h, h, 5.0, 60.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatex(0, 0, -10<<16); //-40<<16);
}

static void destroy_window(void)
{
	glClose();
}

static void event_loop(void)
{
	while (1) {
#if 0
		while (XPending(dpy) > 0) {
			XEvent event;
			XNextEvent(dpy, &event);
			switch (event.type) {
				case KeyPress:
					{
						char buffer[10];
						int r, code;
						code = XLookupKeysym(&event.xkey, 0);
						if (code == XK_Left) {
							view_roty += 5<<16;
						}
						else if (code == XK_Right) {
							view_roty -= 5<<16;
						}
						else if (code == XK_Up) {
							view_rotx += 5<<16;
						}
						else if (code == XK_Down) {
							view_rotx -= 5<<16;
						}
						else {
							r = XLookupString(&event.xkey, buffer, sizeof(buffer),
									NULL, NULL);
							if (buffer[0] == 27) {
								/* escape */
								return;
							}
						}
					}
			}
		}
#endif

		/* next frame */
		angle += 1<<8;//2<<16;

		draw();
		glSwapBuffers();

		/* calc framerate */
		{
			static int t0 = -1;
			static int frames = 0;
			int t = current_time();

			if (t0 < 0)
				t0 = t;

			frames++;

			if (t - t0 >= 1) {
				int seconds = t - t0;
				int fps = frames / seconds;
				printf("%d frames in %d seconds = %d FPS\n", frames, seconds, fps);
				t0 = t;
				frames = 0;
			}
		}
	}
}


int main(void)
{
	Fix_trig_init();
	make_window();

	printf("GL_RENDERER   = %s\n", (char *) glGetString(GL_RENDERER));
	printf("GL_VERSION    = %s\n", (char *) glGetString(GL_VERSION));
	printf("GL_VENDOR     = %s\n", (char *) glGetString(GL_VENDOR));
	printf("GL_EXTENSIONS = %s\n", (char *) glGetString(GL_EXTENSIONS));

	init();

	event_loop();

	destroy_window();

	return 0;
}