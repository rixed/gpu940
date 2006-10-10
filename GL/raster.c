#include "gli.h"

/*
 * Data Definitions
 */

static GLfixed point_size, line_width;
static GLenum cull_face;

/*
 * Private Functions
 */

/*
 * Public Functions
 */

int gli_raster_begin(void)
{
	point_size = 1<<16;
	line_width = 1<<16;
	cull_face = GL_BACK;
	return 0;
}

extern inline void gli_raster_end(void);

void glPointSizex(GLfixed size)
{
	if (size <= 0) {
		return gli_set_error(GL_INVALID_VALUE);
	}
	point_size = size;
}

void glLineWidthx(GLfixed width)
{
	if (width <= 0) {
		return gli_set_error(GL_INVALID_VALUE);
	}
	line_width = width;
}

void glCullFace(GLenum mode)
{
	if (mode < GL_FRONT || mode > GL_FRONT_AND_BACK) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	cull_face = mode;
}

