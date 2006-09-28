AM_CFLAGS = -I $(top_srcdir)/include -fstrict-aliasing -D_GNU_SOURCE -std=c99 -Wall -W -pedantic -pipe

noinst_PROGRAMS = sample1 sample2 codealone
sample1_SOURCES = sample1.c
sample2_SOURCES = sample2.c
codealone_SOURCES = codealone.c pics.h

sample1_LDADD = ../lib/libgpu940.la
sample2_LDADD = ../lib/libgpu940.la
codealone_LDADD = ../lib/libgpu940.la
codealone_LDFLAGS = -static

gimpcomp: gimpcomp.c title.c greetz.c never.c gpu940_pic.c
	@HOST_CC@ -std=c99 -DGNU_SOURCES -Wall -W $< -o $@

pics.h: gimpcomp title.c greetz.c never.c gpu940_pic.c
	./gimpcomp > $@

clean-local:
	rm -f gimpcomp pics.h
