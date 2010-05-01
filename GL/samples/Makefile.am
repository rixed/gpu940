AM_CFLAGS = -I $(top_srcdir)/include -fstrict-aliasing -D_GNU_SOURCE -std=c99 -Wall -W -pedantic -pipe

noinst_PROGRAMS = gpugears texobj lines
gpugears_SOURCES = gpugears.c
gpugears_LDADD = ../libGL.la ../../lib/libgpu940.la
gpugears_LDFLAGS = -static

texobj_SOURCES = texobj.c
texobj_LDADD = ../libGL.la ../../lib/libgpu940.la
texobj_LDFLAGS = -static

lines_SOURCES = lines.c
lines_LDADD = ../libGL.la ../../lib/libgpu940.la
lines_LDFLAGS = -static

