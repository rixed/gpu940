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
#ifndef GLI_061212
#define GLI_061212

/* This file tries to convert legacy opengl calls involving floats to their
 * fixed point counter-part. Our maximum representable value beeing 2^15, and
 * maximum usable value beeing even much less, needless to say the openGl
 * specifications requirements for floats are not met.
 *
 * We use inline functions instead of defines for better type checking.
 * Hope that GCC will make float disapear altogether despite of this.
 * In order to partially check this assertion, we do not provide static
 * version of these.
 * You are not encouraged to use the v version of these functions, because
 * there will be no way for the compiler to wipe out the floats, then.
 * You are not encouraged to use other versions neither, anyway. Use
 * fixed points natively if you can !
 */

typedef float GLfloat;
typedef double GLdouble;
#define f2x(x) ((x)*65536)

static inline void glVertex2f(GLfloat x, GLfloat y)
{
	glVertex2x(f2x(x), f2x(y));
}
static inline void glVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
	glVertex3x(f2x(x), f2x(y), f2x(z));
}
static inline void glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	glVertex4x(f2x(x), f2x(y), f2x(z), f2x(w));
}
static inline void glVertex2fv(GLfloat const *v)
{
	glVertex2x(f2x(v[0]), f2x(v[1]));
}
static inline void glVertex3fv(GLfloat const *v)
{
	glVertex3x(f2x(v[0]), f2x(v[1]), f2x(v[2]));
}
static inline void glVertex4fv(GLfloat const *v)
{
	glVertex4x(f2x(v[0]), f2x(v[1]), f2x(v[2]), f2x(v[3]));
}
static inline void glVertex2d(GLdouble x, GLdouble y)
{
	glVertex2f(x, y);
}
static inline void glVertex3d(GLdouble x, GLdouble y, GLdouble z)
{
	glVertex3f(x, y, z);
}
static inline void glVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
	glVertex4f(x, y, z, w);
}
static inline void glVertex2dv(GLdouble const *v)
{
	glVertex2x(f2x(v[0]), f2x(v[1]));
}
static inline void glVertex3dv(GLdouble const *v)
{
	glVertex3x(f2x(v[0]), f2x(v[1]), f2x(v[2]));
}
static inline void glVertex4dv(GLdouble const *v)
{
	glVertex4x(f2x(v[0]), f2x(v[1]), f2x(v[2]), f2x(v[3]));
}

static inline void glTexCoord1f(GLfloat s)
{
	glTexCoord1x(f2x(s));
}
static inline void glTexCoord2f(GLfloat s, GLfloat t)
{
	glTexCoord2x(f2x(s), f2x(t));
}
static inline void glTexCoord3f(GLfloat s, GLfloat t, GLfloat r)
{
	glTexCoord3x(f2x(s), f2x(t), f2x(r));
}
static inline void glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
	glTexCoord4x(f2x(s), f2x(t), f2x(r), f2x(q));
}
static inline void glTexCoord1fv(GLfloat const *v)
{
	glTexCoord1x(f2x(v[0]));
}
static inline void glTexCoord2fv(GLfloat const *v)
{
	glTexCoord2x(f2x(v[0]), f2x(v[1]));
}
static inline void glTexCoord3fv(GLfloat const *v)
{
	glTexCoord3x(f2x(v[0]), f2x(v[1]), f2x(v[2]));
}
static inline void glTexCoord4fv(GLfloat const *v)
{
	glTexCoord4x(f2x(v[0]), f2x(v[1]), f2x(v[2]), f2x(v[3]));
}
static inline void glTexCoord1d(GLdouble s)
{
	glTexCoord1f(s);
}
static inline void glTexCoord2d(GLdouble s, GLdouble t)
{
	glTexCoord2f(s, t);
}
static inline void glTexCoord3d(GLdouble s, GLdouble t, GLdouble r)
{
	glTexCoord3f(s, t, r);
}
static inline void glTexCoord4d(GLdouble s, GLdouble t, GLdouble r, GLdouble q)
{
	glTexCoord4f(s, t, r, q);
}
static inline void glTexCoord1dv(GLdouble const *v)
{
	glTexCoord1x(f2x(v[0]));
}
static inline void glTexCoord2dv(GLdouble const *v)
{
	glTexCoord2x(f2x(v[0]), f2x(v[1]));
}
static inline void glTexCoord3dv(GLdouble const *v)
{
	glTexCoord3x(f2x(v[0]), f2x(v[1]), f2x(v[2]));
}
static inline void glTexCoord4dv(GLdouble const *v)
{
	glTexCoord4x(f2x(v[0]), f2x(v[1]), f2x(v[2]), f2x(v[3]));
}

static inline void glNormal3f(GLfloat x, GLfloat y, GLfloat z)
{
	glNormal3x(f2x(x), f2x(y), f2x(z));
}
static inline void glNormal3fv(GLfloat const *v)
{
	glNormal3x(f2x(v[0]), f2x(v[1]), f2x(v[2]));
}
static inline void glNormal3d(GLdouble x, GLdouble y, GLdouble z)
{
	glNormal3x(f2x(x), f2x(y), f2x(z));
}
static inline void glNormal3dv(GLdouble const *v)
{
	glNormal3x(f2x(v[0]), f2x(v[1]), f2x(v[2]));
}

static inline void glColor3f(GLfloat red, GLfloat green, GLfloat blue)
{
	glColor3x(f2x(red), f2x(green), f2x(blue));
}
static inline void glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	glColor4x(f2x(red), f2x(green), f2x(blue), f2x(alpha));
}
static inline void glColor3fv(GLfloat const *v)
{
	glColor3x(f2x(v[0]), f2x(v[1]), f2x(v[2]));
}
static inline void glColor4fv(GLfloat const *v)
{
	glColor4x(f2x(v[0]), f2x(v[1]), f2x(v[2]), f2x(v[3]));
}
static inline void glColor3d(GLdouble red, GLdouble green, GLdouble blue)
{
	glColor3x(f2x(red), f2x(green), f2x(blue));
}
static inline void glColor4d(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha)
{
	glColor4x(f2x(red), f2x(green), f2x(blue), f2x(alpha));
}
static inline void glColor3dv(GLdouble const *v)
{
	glColor3x(f2x(v[0]), f2x(v[1]), f2x(v[2]));
}
static inline void glColor4dv(GLdouble const *v)
{
	glColor4x(f2x(v[0]), f2x(v[1]), f2x(v[2]), f2x(v[3]));
}

#define mx(m) GLfixed const mx[16] = { \
	f2x(m[ 0]), f2x(m[ 1]), f2x(m[ 2]), f2x(m[ 3]), \
	f2x(m[ 4]), f2x(m[ 5]), f2x(m[ 6]), f2x(m[ 7]), \
	f2x(m[ 8]), f2x(m[ 9]), f2x(m[10]), f2x(m[11]), \
	f2x(m[12]), f2x(m[13]), f2x(m[14]), f2x(m[15]) \
}
static inline void glLoadMatrixf(GLfloat const *m)
{
	mx(m);
	glLoadMatrixx(mx);
}
static inline void glLoadMatrixd(GLdouble const *m)
{
	mx(m);
	glLoadMatrixx(mx);
}
static inline void glMultMatrixf(GLfloat const *m)
{
	mx(m);
	glMultMatrixx(mx);
}
static inline void glMultMatrixd(GLdouble const *m)
{
	mx(m);
	glMultMatrixx(mx);
}
#undef mx
static inline void glRotatef(GLfloat ang, GLfloat x, GLfloat y, GLfloat z)
{
	glRotatex(f2x(ang), f2x(x), f2x(y), f2x(z));
}
static inline void glRotated(GLdouble ang, GLdouble x, GLdouble y, GLdouble z)
{
	glRotatex(f2x(ang), f2x(x), f2x(y), f2x(z));
}
static inline void glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
	glTranslatex(f2x(x), f2x(y), f2x(z));
}
static inline void glTranslated(GLdouble x, GLdouble y, GLdouble z)
{
	glTranslatex(f2x(x), f2x(y), f2x(z));
}
static inline void glScalef(GLfloat x, GLfloat y, GLfloat z)
{
	glScalex(f2x(x), f2x(y), f2x(z));
}
static inline void glScaled(GLdouble x, GLdouble y, GLdouble z)
{
	glScalex(f2x(x), f2x(y), f2x(z));
}
static inline void glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{
	glFrustumx(f2x(l), f2x(r), f2x(b), f2x(t), f2x(n), f2x(f));
}
static inline void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{
	glOrthox(f2x(l), f2x(r), f2x(b), f2x(t), f2x(n), f2x(f));
}

#undef f2x
#endif
