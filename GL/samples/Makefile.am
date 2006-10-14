AM_CFLAGS = -I $(top_srcdir)/include -fstrict-aliasing -D_GNU_SOURCE -std=c99 -Wall -W -pedantic -pipe

bin_PROGRAMS = gpugears
gpugears_SOURCES = gpugears.c
gpugears_LDADD = ../libGL.la ../../lib/libgpu940.la
gpugears_LDFLAGS = -static

