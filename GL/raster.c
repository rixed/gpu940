#include "gli.h"

/*
 * Data Definitions
 */

GLfixed gli_point_size, gli_line_width;
static enum gli_CullFace cull_face;
enum gli_PolyMode gli_polygon_mode;

/*
 * Private Functions
 */

/*
 * Public Functions
 */

int gli_raster_begin(void)
{
	gli_point_size = 1<<16;
	gli_line_width = 1<<16;
	cull_face = GL_BACK;
	gli_polygon_mode = GL_FILL;
	return 0;
}

extern inline void gli_raster_end(void);

void glPointSizex(GLfixed size)
{
	if (size <= 0) {
		return gli_set_error(GL_INVALID_VALUE);
	}
	gli_point_size = size;
}

void glLineWidthx(GLfixed width)
{
	if (width <= 0) {
		return gli_set_error(GL_INVALID_VALUE);
	}
	gli_line_width = width;
}

void glCullFace(GLenum mode)
{
	if (/*mode < GL_FRONT ||*/ mode > GL_FRONT_AND_BACK) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	cull_face = mode;
}

bool gli_must_render_face(enum gli_CullFace face)
{
	if (! gli_enabled(GL_CULL_FACE)) return true;
	return cull_face != GL_FRONT_AND_BACK && cull_face != face;
}

void glPolygonMode(GLenum face, GLenum mode)
{
	if (/*face < GL_FRONT ||*/ face > GL_FRONT_AND_BACK) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	if (/*mode < GL_POINT ||*/ mode > GL_FILL) {
		return gli_set_error(GL_INVALID_ENUM);
	}
	gli_polygon_mode = mode;	// We do not handle handling differently front and back mode (for now, we don't handle polygon mode altogether)
}
