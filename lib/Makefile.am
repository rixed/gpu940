AM_CFLAGS = -I $(top_srcdir)/include -fstrict-aliasing -D_GNU_SOURCE -std=c99 -Wall -W -pedantic -pipe

lib_LTLIBRARIES = libgpu940.la
libgpu940_la_SOURCES = gpu940.c fixmath.c fixtrig.c text.c mm.c input.c
noinst_HEADERS = kernlist.h mm.h input.h

libgpu940_la_LDFLAGS = -version-info 1:0:0 -Wl,--warn-common

