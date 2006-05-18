AM_CFLAGS = -I $(top_srcdir)/include -fstrict-aliasing -D_GNU_SOURCE -std=c99 -Wall -W -pedantic -pipe

lib_LTLIBRARIES = libperftime.la
libperftime_la_SOURCES = perftime.c perftime.h
libperftime_la_LDFLAGS = -Wl,--warn-common

sample: sample.o $(lib)
	$(CC) $(LDFLAGS) $^ -o $@

clean-local:
	rm -f sample

