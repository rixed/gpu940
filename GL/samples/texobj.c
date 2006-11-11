
/*
 * Example of using the 1.1 texture object functions.
 * Also, this demo utilizes Mesa's fast texture map path.
 *
 * Brian Paul   June 1996   This file is in the public domain.
 */
/*
 * Ported to gpu940 as a test for textures
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "GL/gl.h"
#include "fixmath.h"

static GLuint TexObj[2];
static GLfixed Angle = 0;


#if defined(GL_VERSION_1_1) || defined(GL_VERSION_1_2)
#  define TEXTURE_OBJECT 1
#elif defined(GL_EXT_texture_object)
#  define TEXTURE_OBJECT 1
#  define glBindTexture(A,B)     glBindTextureEXT(A,B)
#  define glGenTextures(A,B)     glGenTexturesEXT(A,B)
#  define glDeleteTextures(A,B)  glDeleteTexturesEXT(A,B)
#endif




static void draw( void )
{
	static GLfixed const vertexes[4*2] = {
     -1<<16, -1<<16,
      1<<16, -1<<16,
      1<<16,  1<<16,
     -1<<16,  1<<16,
	};
	static GLfixed const texcoords[4*2] = {
      0, 0,
      1<<16, 0,
      1<<16, 1<<16,
      0, 1<<16,
	};
	glEnableClientState( GL_VERTEX_ARRAY );
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	glVertexPointer( 2, GL_FIXED, 0, vertexes );
	glTexCoordPointer( 2, GL_FIXED, 0, texcoords );
	
	glClear( GL_COLOR_BUFFER_BIT );

   glColor3x( 1<<16, 1<<16, 1<<16 );

   /* draw first polygon */
   glPushMatrix();
   glTranslatex( -1<<16, 0, 0 );
   glRotatex( Angle, 0, 0, 1<<16 );
   glBindTexture( GL_TEXTURE_2D, TexObj[0] );
   glDrawArrays( GL_QUADS, 0, 4 );
	glPopMatrix();

   /* draw second polygon */
   glPushMatrix();
   glTranslatex( 1<<16, 0, 0 );
   glRotatex( Angle-(1<<14), 0, 1<<16, 0 );
   glBindTexture( GL_TEXTURE_2D, TexObj[1] );
   glDrawArrays( GL_QUADS, 0, 4 );
   glPopMatrix();

   (void)glSwapBuffers();
}



static int
gettime()
{
	static int mytime = 0;
   return mytime += 100;
}

static void idle( void )
{
   static int t0 = -1.;
   int t, dt;
   if (t0 < 0) t0 = gettime();
   t = gettime();
   dt = t - t0;
   t0 = t;
   Angle += dt;
   //glutPostRedisplay();
}


/* new window size or exposure */
static void reshape( int width, int height )
{
   glViewport(0, 0, (GLint)width, (GLint)height);
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   /*   glOrtho( -3.0, 3.0, -3.0, 3.0, -10.0, 10.0 );*/
   glFrustumx( -2<<16, 2<<16, -2<<16, 2<<16, 6<<16, 20<<16 );
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
   glTranslatex( 0, 0, -8<<16 );
}


static void init( void )
{
   static int width=8, height=8;
   static GLubyte tex1[] = {
     0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 1, 0, 0, 0,
     0, 0, 0, 1, 1, 0, 0, 0,
     0, 0, 0, 0, 1, 0, 0, 0,
     0, 0, 0, 0, 1, 0, 0, 0,
     0, 0, 0, 0, 1, 0, 0, 0,
     0, 0, 0, 1, 1, 1, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0 };

   static GLubyte tex2[] = {
     0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 2, 2, 0, 0, 0,
     0, 0, 2, 0, 0, 2, 0, 0,
     0, 0, 0, 0, 0, 2, 0, 0,
     0, 0, 0, 0, 2, 0, 0, 0,
     0, 0, 0, 2, 0, 0, 0, 0,
     0, 0, 2, 2, 2, 2, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0 };

   GLubyte tex[64][3];
   GLint i, j;


   glDisable( GL_DITHER );

   /* Setup texturing */
   glEnable( GL_TEXTURE_2D );
   glTexEnvx( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );
   glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST );


   /* generate texture object IDs */
   glGenTextures( 2, TexObj );

   /* setup first texture object */
   glBindTexture( GL_TEXTURE_2D, TexObj[0] );
   assert(glIsTexture(TexObj[0]));
   /* red on white */
   for (i=0;i<height;i++) {
      for (j=0;j<width;j++) {
         int p = i*width+j;
         if (tex1[(height-i-1)*width+j]) {
            tex[p][0] = 255;   tex[p][1] = 0;     tex[p][2] = 0;
         }
         else {
            tex[p][0] = 255;   tex[p][1] = 255;   tex[p][2] = 255;
         }
      }
   }

   glTexImage2D( GL_TEXTURE_2D, 0, 3, width, height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, tex );
   glTexParameterx( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
   glTexParameterx( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
   glTexParameterx( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
   glTexParameterx( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
   /* end of texture object */

   /* setup second texture object */
   glBindTexture( GL_TEXTURE_2D, TexObj[1] );
   assert(glIsTexture(TexObj[1]));
   assert(!glIsTexture(TexObj[1] + 999));
   /* green on blue */
   for (i=0;i<height;i++) {
      for (j=0;j<width;j++) {
         int p = i*width+j;
         if (tex2[(height-i-1)*width+j]) {
            tex[p][0] = 0;     tex[p][1] = 255;   tex[p][2] = 0;
         }
         else {
            tex[p][0] = 0;     tex[p][1] = 0;     tex[p][2] = 255;
         }
      }
   }
   glTexImage2D( GL_TEXTURE_2D, 0, 3, width, height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, tex );
   glTexParameterx( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
   glTexParameterx( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
   glTexParameterx( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
   glTexParameterx( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
   /* end texture object */

}



int main( void )
{
   if (GL_TRUE != glOpen(0)) {
		exit(1);
	}

   /* check that renderer has the GL_EXT_texture_object extension
    * or supports OpenGL 1.1
    */
   {
      char *exten = (char *) glGetString( GL_EXTENSIONS );
      char *version = (char *) glGetString( GL_VERSION );
      if ( !strstr( exten, "GL_EXT_texture_object" )
          && strncmp( version, "1.1", 3 )!=0
          && strncmp( version, "1.2", 3 )!=0 ) {
			exit(1);
      }
   }

   init();

   reshape(320, 240);
	do {
		idle();
		draw();
	} while (1);

   glDeleteTextures( 2, TexObj );
   glClose();
   return 0;
}
