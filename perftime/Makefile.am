AM_CFLAGS = -I $(top_srcdir)/include -fstrict-aliasing -D_GNU_SOURCE -std=c99 -Wall -W -pedantic -pipe

lib_LIBRARIES = libperftime.a
libperftime_a_SOURCES = perftime.c perftime.h

sample: sample.o $(lib)
	$(CC) $(LDFLAGS) $^ -o $@

clean-local:
	rm -f sample

