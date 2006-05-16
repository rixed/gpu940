AM_CFLAGS = -I $(top_srcdir)/include -fstrict-aliasing -D_GNU_SOURCE -std=c99 -Wall -W -pedantic -pipe

lib_LTLIBRARIES = libgpu940.la
libgpu940_la_SOURCES = gpu940.c fixmath.c text.c mm.c

libgpu940_la_LDFLAGS = -version-info 1:0:0 -Wl,--warn-common

