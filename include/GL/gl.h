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
#include <stdbool.h>
#include <unistd.h>

// GL types
typedef enum { GL_FALSE, GL_TRUE } GLboolean;
typedef unsigned GLenum;
typedef uint8_t GLubyte;
typedef int8_t GLbyte;
typedef uint16_t GLushort;
typedef int16_t GLshort;
typedef int GLint;
typedef ssize_t GLsizei;
typedef void GLvoid;
typedef int32_t GLfixed;
typedef int32_t GLclampx;
typedef unsigned GLbitfield;
typedef unsigned GLuint;

// Replaces GLX, EGL, ...
enum glOpen_attribs { DEPTH_BUFFER = 1 };
GLboolean glOpen(unsigned mask);
void glClose(void);
GLboolean glSwapBuffers(void);

// Primitives

// Vertex Arrays
#define GL_TEXTURE0 0
#define GL_TEXTURE1 1
#define GL_TEXTURE2 2
#define GL_TEXTURE3 3
enum gli_Types { GL_FLOAT, GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, GL_UNSIGNED_INT, GL_FIXED, GL_BYTE, GL_SHORT };
void glColorPointer(GLint size, GLenum type, GLsizei stride, GLvoid const *pointer);
void glNormalPointer(GLenum type, GLsizei stride, GLvoid const *pointer);
void glVertexPointer(GLint size, GLenum type, GLsizei stride, GLvoid const *pointer);
void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, GLvoid const *pointer);
void glClientActiveTexture(GLenum texture);
enum gli_ClientState { GL_COLOR_ARRAY, GL_NORMAL_ARRAY, GL_VERTEX_ARRAY, GL_TEXTURE_COORD_ARRAY };
void glEnableClientState(GLenum array);
void glDisableClientState(GLenum array);
enum gli_DrawMode { GL_POINTS, GL_LINE_STRIP, GL_LINE_LOOP, GL_LINES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN, GL_TRIANGLES, GL_QUAD_STRIP, GL_QUADS, GL_POLYGON };
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
extern GLfixed gli_current_color[4];
extern GLfixed gli_current_normal[3];
extern GLfixed gli_current_texcoord[4];
static inline void glColor4x(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha)
{
	gli_current_color[0] = red;
	gli_current_color[1] = green;
	gli_current_color[2] = blue;
	gli_current_color[3] = alpha;
}
static inline void glColor3x(GLfixed red, GLfixed green, GLfixed blue)
{
	glColor4x(red, green, blue, 1U<<16);
}
static inline void glColor3xv(GLfixed const *v)
{
	glColor4x(v[0], v[1], v[2], 1<<16);
}
static inline void glColor4xv(GLfixed const *v)
{
	glColor4x(v[0], v[1], v[2], v[3]);
}
static inline void glNormal3x(GLfixed nx, GLfixed ny, GLfixed nz)
{
	gli_current_normal[0] = nx;
	gli_current_normal[1] = ny;
	gli_current_normal[2] = nz;
}
static inline void glNormal3xv(GLfixed const *v)
{
	glNormal3x(v[0], v[1], v[2]);
}
static inline void glTexCoord4x(GLfixed s, GLfixed t, GLfixed r, GLfixed q)
{
	gli_current_texcoord[0] = s;
	gli_current_texcoord[1] = t;
	gli_current_texcoord[2] = r;
	gli_current_texcoord[3] = q;
}
static inline void glTexCoord1x(GLfixed s)
{
	glTexCoord4x(s, 0, 0, 1<<16);
}
static inline void glTexCoord2x(GLfixed s, GLfixed t)
{
	glTexCoord4x(s, t, 0, 1<<16);
}
static inline void glTexCoord3x(GLfixed s, GLfixed t, GLfixed r)
{
	glTexCoord4x(s, t, r, 1<<16);
}
static inline void glTexCoord1xv(GLfixed const *v)
{
	glTexCoord4x(v[0], 0, 0, 1<<16);
}
static inline void glTexCoord2xv(GLfixed const *v)
{
	glTexCoord4x(v[0], v[1], 0, 1<<16);
}
static inline void glTexCoord3xv(GLfixed const *v)
{
	glTexCoord4x(v[0], v[1], v[2], 1<<16);
}
static inline void glTexCoord4xv(GLfixed const *v)
{
	glTexCoord4x(v[0], v[1], v[2], v[3]);
}
#define GL_LIGHT0 1
#define GL_LIGHT1 2
#define GL_LIGHT2 3
#define GL_LIGHT3 4
#define GL_LIGHT4 5
#define GL_LIGHT5 6
#define GL_LIGHT6 7
#define GL_LIGHT7 8
enum gli_ColorParam {
	GL_SPOT_EXPONENT, GL_SPOT_CUTOFF, GL_CONSTANT_ATTENUATION, GL_LINEAR_ATTENUATION, GL_QUADRATIC_ATTENUATION,
	GL_AMBIENT, GL_DIFFUSE, GL_SPECULAR, GL_POSITION, GL_SPOT_DIRECTION,
	GL_SHININESS, GL_EMISSION, GL_AMBIENT_AND_DIFFUSE
};
void glLightx(GLenum light, GLenum pname, GLfixed param);
void glLightxv(GLenum light, GLenum pname, GLfixed const *params);
void glMaterialx(GLenum face, GLenum pname, GLfixed param);
void glMaterialxv(GLenum face, GLenum pname, GLfixed const *params);
enum gli_LightModParam { GL_LIGHT_MODEL_AMBIENT, GL_LIGHT_MODEL_TWO_SIDE };
void glLightModelx(GLenum pname, GLfixed param);
void glLightModelxv(GLenum pname, GLfixed const *params);
enum gli_ShadeModel { GL_FLAT, GL_SMOOTH };
void glShadeModel(GLenum mode);
enum gli_FrontFace { GL_CW, GL_CCW };
void glFrontFace(GLenum mode);

// Rasterization
void glPointSizex(GLfixed size);
void glLineWidthx(GLfixed width);
enum gli_CullFace { GL_FRONT, GL_BACK, GL_FRONT_AND_BACK };
void glCullFace(GLenum mode);
enum gli_PolyMode { GL_POINT, GL_LINE, GL_FILL };
void glPolygonMode(GLenum face, GLenum mode);

// Pixel Operations

// Textures
enum gli_TexParam { GL_TEXTURE_MIN_FILTER, GL_TEXTURE_MAG_FILTER, GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T };
enum gli_TexFilter { GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR };
enum gli_TexWrap { GL_CLAMP, GL_CLAMP_TO_EDGE, GL_REPEAT };
void glTexParameterx(GLenum target, GLenum pname, GLfixed param);
#define GL_TEXTURE_ENV 0
enum gli_TexEnv { GL_TEXTURE_ENV_MODE, GL_TEXTURE_ENV_COLOR };
enum gli_TexEnvMode { GL_MODULATE, GL_DECAL, GL_REPLACE };
void glTexEnvx(GLenum target, GLenum pname, GLfixed param);
void glTexEnvxv(GLenum target, GLenum pname, GLfixed const *params);
enum gli_TexFormat { GL_ALPHA, GL_RGB, GL_RGBA, GL_LUMINANCE, GL_LUMINANCE_ALPHA };
enum gli_TexType { GL_UNSIGNED_SHORT_5_6_5=0x1000 /* avoid collision with gli_Types */, GL_UNSIGNED_SHORT_4_4_4_4, GL_UNSIGNED_SHORT_5_5_5_1 };
void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, GLvoid const *pixels);
void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, void *data);
void glBindTexture(GLenum target, GLuint texture);
void glDeleteTextures(GLsizei n, GLuint const *textures);
void glActiveTexture(GLenum texture);
void glGenTextures(GLsizei n, GLuint *textures);
GLboolean glIsTexture(GLuint texture);

// Fog
enum gli_FogPname { GL_FOG_MODE, GL_FOG_DENSITY, GL_FOG_START, GL_FOG_END, GL_FOG_COLOR };
enum gli_FogMode { GL_EXP=0x100 /* avoid collision with gli_TexFilter for GL_LINEAR */, GL_EXP2 };
void glFogi(GLenum pname, GLint param);
void glFogiv(GLenum pname, GLint const *params);
void glFogx(GLenum pname, GLfixed param);
void glFogxv(GLenum pname, GLfixed const *params);

// Frame Buffer Operations
void glScissor(GLint x, GLint y, GLsizei width, GLsizei height);
enum gli_Func { GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL, GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS };
void glAlphaFuncx(GLenum func, GLclampx ref);
void glStencilFunc(GLenum func, GLint ref, GLuint mask);
enum gli_StencilOp {
	GL_KEEP=0x200 /* avoid conflict with gli_TexEnvMode which also define GL_REPLACE, and gli_BlendFunc for GL_ZERO */,
	GL_INCR, GL_DECR
};
void glStencilOp(GLenum fail, GLenum zfail, GLenum zpass);
void glDepthFunc(GLenum func);
enum gli_BlendFunc {
	GL_ZERO, GL_ONE, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA,
	GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_SRC_ALPHA_SATURATE };
void glBlendFunc(GLenum sfactor, GLenum dfactor);
enum gli_LogicOp { GL_CLEAR, GL_SET, GL_COPY, GL_COPY_INVERTED, GL_NOOP, GL_INVERT, GL_AND,
	GL_NAND, GL_OR, GL_NOR, GL_XOR, GL_EQUIV, GL_AND_REVERSE, GL_AND_INVERTED, GL_OR_REVERSE, GL_OR_INVERTED };
void glLogicOp(GLenum opcode);
#define GL_COLOR_BUFFER_BIT 1
#define GL_DEPTH_BUFFER_BIT 2
#define GL_STENCIL_BUFFER_BIT 4
void glClear(GLbitfield mask);
void glClearColorx(GLclampx red, GLclampx green, GLclampx blue, GLclampx alpha);
void glClearDepthx(GLclampx depth);
void glClearStencil(GLint s);
void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
void glDepthMask(GLboolean flag);
void glStencilMask(GLuint mask);
void glSampleCoveragex(GLclampx value, GLboolean invert);

// Modes and Execution
enum gli_Capability {
	/* GL_LIGHTi */
	GL_ALPHA_TEST=0x1000 /* To skip GL_LIGHTs and allow GL_BLEND in TexEnvMode */,
	GL_BLEND,
	GL_COLOR_LOGIC_OP,
	GL_COLOR_MATERIAL,
	GL_CULL_FACE,
	GL_DEPTH_TEST,
	GL_DITHER,
	GL_FOG,
	GL_LIGHTING,
	GL_POINT_SMOOTH,
	GL_LINE_SMOOTH,
	GL_POLYGON_SMOOTH,
	GL_LINE_STIPPLE,
	GL_POLYGON_STIPPLE,
	GL_MULTISAMPLE,
	GL_NORMALIZE,
	GL_AUTO_NORMAL,
	GL_POLYGON_OFFSET_FILL,
	GL_RESCALE_NORMAL,
	GL_SAMPLE_ALPHA_TO_MASK,
	GL_SAMPLE_ALPHA_TO_ONE,
	GL_SAMPLE_MASK,
	GL_SCISSOR_TEST,
	GL_STENCIL_TEST,
	GL_TEXTURE_2D,
};
void glEnable(GLenum cap);
void glDisable(GLenum cap);
void glFinish(void);
static inline void glFlush(void) {}
enum gli_HintTarget { GL_PERSPECTIVE_CORRECTION_HINT, GL_POINT_SMOOTH_HINT, GL_LINE_SMOOTH_HINT, GL_POLYGON_SMOOTH_HINT, GL_FOG_HINT, NB_HINT_TARGETS };
enum gli_HintMode { GL_FASTEST, GL_NICEST, GL_DONT_CARE };
void glHint(GLenum target, GLenum mode);

// State Queries
enum gli_StringName { GL_VENDOR, GL_RENDERER, GL_VERSION, GL_EXTENSIONS };
GLubyte const *glGetString(GLenum name);
enum gli_Errors { GL_NO_ERROR, GL_INVALID_ENUM, GL_INVALID_VALUE, GL_INVALID_OPERATION, GL_STACK_OVERFLOW, GL_STACK_UNDERFLOW, GL_OUT_OF_MEMORY };
GLenum glGetError(void);
enum gli_ParamName {
	// those are for GetInteger
	GL_ALIASED_POINT_SIZE_RANGE, GL_ALIASED_LINE_WIDTH_RANGE, GL_ALPHA_BITS, GL_BLUE_BITS, GL_COMPRESSED_TEXTURE_FORMATS,
	GL_DEPTH_BITS, GL_GREEN_BITS, GL_IMPLEMENTATION_COLOR_READ_FORMAT_OES, GL_IMPLEMENTATION_COLOR_READ_TYPE_OES,
	GL_MAX_ELEMENTS_INDICES, GL_MAX_ELEMENTS_VERTICES, GL_MAX_LIGHTS, GL_MAX_MODELVIEW_STACK_DEPTH, GL_MAX_PROJECTION_STACK_DEPTH,
	GL_MAX_TEXTURE_SIZE, GL_MAX_TEXTURE_STACK_DEPTH, GL_MAX_TEXTURE_UNITS, GL_MAX_VIEWPORT_DIMS, GL_NUM_COMPRESSED_TEXTURE_FORMATS,
	GL_RED_BITS, GL_SMOOTH_LINE_WIDTH_RANGE, GL_SMOOTH_POINT_SIZE_RANGE, GL_STENCIL_BITS, GL_SUBPIXEL_BITS, GL_VIEWPORT, GL_STEREO,
	GL_SHADE_MODEL,
	// Those are for GetFixed
	GL_LINE_WIDTH,
	// TODO: add more
};
void glGetIntegerv(GLenum pname, GLint *params);
void glGetFixedv(GLenum pname, GLfixed *params);

GLboolean glIsEnabled(GLenum mode);

// Begin/End paradigm
void glBegin(GLenum mode);
void glEnd(void);
void glVertex4xv(GLfixed const *v);
static inline void glVertex4x(GLfixed x, GLfixed y, GLfixed z, GLfixed w)
{
	int32_t v[4] = { x, y, z, w };
	glVertex4xv(v);
}
static inline void glVertex2x(GLfixed x, GLfixed y)
{
	glVertex4x(x, y, 0, 1<<16);
}
static inline void glVertex3x(GLfixed x, GLfixed y, GLfixed z)
{
	glVertex4x(x, y, z, 1<<16);
}
static inline void glVertex2xv(GLfixed const *v)
{
	glVertex4x(v[0], v[1], 0, 1<<16);
}
static inline void glVertex3xv(GLfixed const *v)
{
	glVertex4x(v[0], v[1], v[2], 1<<16);
}

// OES Extensions

// Quick and dirty definitions to help porting of openGl apps using floats
#include "GL/gl_float.h"

#endif
