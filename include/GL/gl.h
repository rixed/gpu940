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
#ifndef GL_H_061009
#define GL_H_061009

#include <stdlib.h>
#include <inttypes.h>

// GL types
typedef enum { GL_FALSE, GL_TRUE } glBool;
typedef unsigned GLenum;
typedef uint8_t GLubyte;
typedef int GLint;
typedef ssize_t GLsizei;
typedef void GLvoid;
typedef int32_t GLfixed;
typedef int32_t GLclampx;

// Replaces GLX, EGL, ...
glBool glOpen(void);
void glClose(void);

// Vertex Arrays
#include <GL/texturenames.h>
enum gli_Types { GL_FLOAT=0, GL_UNSIGNED_BYTE, GL_FIXED, GL_BYTE, GL_SHORT };
void glColorPointer(GLint size, GLenum type, GLsizei stride, GLvoid const *pointer);
void glNormalPointer(GLenum type, GLsizei stride, GLvoid const *pointer);
void glVertexPointer(GLint size, GLenum type, GLsizei stride, GLvoid const *pointer);
void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, GLvoid const *pointer);
void glClientActiveTexture(GLenum texture);
enum gli_ClientState { GL_COLOR_ARRAY, GL_NORMAL_ARRAY, GL_VERTEX_ARRAY, GL_TEXTURE_COORD_ARRAY };
void glEnableClientState(GLenum array);
void glDisableClientState(GLenum array);
enum gli_DrawMode { GL_POINTS, GL_LINE_STRIP, GL_LINE_LOOP, GL_LINES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN, GL_TRIANGLES };
void glDrawArrays(GLenum mode, GLint first, GLsizei count);
void glDrawElements(GLenum mode, GLsizei count, GLenum type, GLvoid const *indices);

// Coordinate Transformation
enum gli_MatrixMode { GL_MODELVIEW, GL_PROJECTION, GL_TEXTURE };
void glMatrixMode(GLenum mode);
void glPushMatrix(void);
void glPopMatrix(void);
void glLoadMatrixx(GLfixed const *m);
void glLoadIdentity(void);
void glRotatex(GLfixed angle, GLfixed x, GLfixed y, GLfixed z);
void glMultMatrixx(GLfixed const *m);
void glScalex(GLfixed x, GLfixed y, GLfixed z);
void glTranslatex(GLfixed x, GLfixed y, GLfixed z);
void glFrustumx(GLfixed left, GLfixed right, GLfixed bottom, GLfixed top, GLfixed near, GLfixed far);
void glOrthox(GLfixed left, GLfixed right, GLfixed bottom, GLfixed top, GLfixed near, GLfixed far);
void glDepthRangex(GLclampx near, GLclampx far);
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height);

// Color and Lighting
void glColor4x(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha);
void glNormal3x(GLfixed nx, GLfixed ny, GLfixed nz);
#include <GL/lightnames.h>
enum gli_ColorParam {
	GL_SPOT_EXPONENT, GL_SPOT_CUTOFF, GL_CONSTANT_ATTENUATION, GL_LINEAR_ATTENUATION, GL_QUADRATIC_ATTENUATION,
	GL_AMBIENT, GL_DIFFUSE, GL_SPECULAR, GL_POSITION, GL_SPOT_DIRECTION,
	GL_SHININESS, GL_EMISSION, GL_AMBIENT_AND_DIFFUSE
};
void glLightx(GLenum light, GLenum pname, GLfixed param);
void glLightxv(GLenum light, GLenum pname, GLfixed const *params);
enum gli_MatFace { GL_FRONT_AND_BACK };
void glMaterialx(GLenum face, GLenum pname, GLfixed param);
void glMaterialxv(GLenum face, GLenum pname, GLfixed const *params);
enum gli_LightModParam { GL_LIGHT_MODEL_AMBIENT, GL_LIGHT_MODEL_TWO_SIDE };
void glLightModelx(GLenum pname, GLfixed param);
void glLightModelxv(GLenum pname, GLfixed const *params);
enum gli_ShadeModel { GL_FLAT, GL_SMOOTH };
void glShadeModel(GLenum mode);
enum gli_FrontFace { GL_CW, GL_CCW };
void glFrontFace(GLenum mode);

// State Queries
enum gli_StringName { GL_VENDOR, GL_RENDERER, GL_VERSION, GL_EXTENSIONS };
GLubyte const *glGetString(GLenum name);
enum gli_Errors { GL_NO_ERROR, GL_INVALID_ENUM, GL_INVALID_VALUE, GL_INVALID_OPERATION, GL_STACK_OVERFLOW, GL_STACK_UNDERFLOW, GL_OUT_OF_MEMORY };
GLenum glGetError(void);
enum gli_ParamName {
	GL_ALIASED_POINT_SIZE_RANGE, GL_ALIASED_LINE_WIDTH_RANGE, GL_ALPHA_BITS, GL_BLUE_BITS, GL_COMPRESSED_TEXTURE_FORMATS,
	GL_DEPTH_BITS, GL_GREEN_BITS, GL_IMPLEMENTATION_COLOR_READ_FORMAT_OES, GL_IMPLEMENTATION_COLOR_READ_TYPE_OES,
	GL_MAX_ELEMENTS_INDICES, GL_MAX_ELEMENTS_VERTICES, GL_MAX_LIGHTS, GL_MAX_MODELVIEW_STACK_DEPTH, GL_MAX_PROJECTION_STACK_DEPTH,
	GL_MAX_TEXTURE_SIZE, GL_MAX_TEXTURE_STACK_DEPTH, GL_MAX_TEXTURE_UNITS, GL_MAX_VIEWPORT_DIMS, GL_NUM_COMPRESSED_TEXTURE_FORMATS,
	GL_RED_BITS, GL_SMOOTH_LINE_WIDTH_RANGE, GL_SMOOTH_POINT_SIZE_RANGE, GL_STENCIL_BITS, GL_SUBPIXEL_BITS,
};
void glGetIntegerv(GLenum pname, GLint *params);

#endif
