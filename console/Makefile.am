AM_CFLAGS = -I $(top_srcdir)/include -fstrict-aliasing -D_GNU_SOURCE -std=c99 -Wall -W -pedantic -pipe

lib_LIBRARIES = libconsole.a
libconsole_a_SOURCES = console.c console.h

font.c: cp850-8x8.psf psf2c
	./psf2c $< > $@
