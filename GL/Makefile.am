SUBDIRS = . samples

AM_CFLAGS = -I $(top_srcdir)/include -fstrict-aliasing -D_GNU_SOURCE -std=c99 -Wall -W -pipe

lib_LTLIBRARIES = libGL.la
libGL_la_SOURCES = \
	arrays.c colors.c egl.c framebuf.c modes.c raster.c state.c texture.c transfo.c begend.c cmd.c fog.c \
	gli.h framebuf.h modes.h raster.h state.h texture.h transfo.h arrays.h colors.h begend.h cmd.h fog.h

libGL_la_LIBADD = ../lib/libgpu940.la
libGL_la_LDFLAGS = -version-info 1:0:0 -Wl,--warn-common

