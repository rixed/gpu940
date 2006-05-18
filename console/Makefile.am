AM_CFLAGS = -I $(top_srcdir)/include -fstrict-aliasing -D_GNU_SOURCE -std=c99 -Wall -W -pedantic -pipe

lib_LTLIBRARIES = libconsole.la
libconsole_la_SOURCES = console.c console.h

font.c: cp850-8x8.psf psf2c
	./psf2c $< > $@
