AM_CFLAGS = -I $(top_srcdir)/include -fstrict-aliasing -D_GNU_SOURCE -std=c99 -Wall -W -pedantic -pipe

bin_PROGRAMS = gpu940 load940
gpu940_SOURCES = \
	gpu940.c \
	gpu940i.h \
	poly.c \
	poly.h \
	clip.c \
	clip.h \
	mylib.c \
	mylib.h \
	text.c \
	text.h \
	raster.c \
	raster.h \
	draw_line.gp2x.S \
	crt0.S \
	mydiv.c
gpu940_LDADD = ../console/libconsole.a ../perftime/libperftime.a ../lib/fixmath.lo
#AM_CFLAGS += -fno-pic -ffreestanding
load940_SOURCES = load940.c

if GP2X
# Trick to use the context structure from within .S files
writeoffs: writeoffs.c gpu940i.h ../include/gpu940.h
#	$(CC) $(CFLAGS) $(AM_CFLAGS) $(LDFLAGS) $(AM_LDFLAGS) -std=c99 -Wall -W -o writeoffs writeoffs.c
	@HOST_CC@ -std=c99 -Wall -W -o $@ $< -I$(top_srcdir)/include
offsets.S: writeoffs $(top_srcdir)/include/gpu940.h $(top_srcdir)/bin/gpu940i.h
	./$< > $@
draw_line.gp2x.o: offsets.S

OBJCOPY = $(GP2XDEVDIR)/bin/arm-linux-objcopy
OBJDUMP = $(GP2XDEVDIR)/bin/arm-linux-objdump
gpu940: $(gpu940_OBJECTS) script.ld $(top_srcdir)/console/libconsole.a $(top_srcdir)/perftime/libperftime.a
	$(LD) -o $@.elf $(LDFLAGS) -L$(top_srcdir)/console -L$(top_srcdir)/perftime -L$(top_srcdir)/lib -T script.ld -N -s
	$(OBJCOPY) --output-target binary --only-section upper32 $@.elf $@
bench940: bench.o bench.ld
	$(LD) -o $@.elf $(LDFLAGS) -T bench.ld -N -s
	$(OBJCOPY) --output-target binary --only-section upper32 $@.elf $@
all: bench940

gpu940.source: gpu940
	$(OBJDUMP) --target=binary --architecture=arm -D $< > $@

clean-local:
	rm -f writeoffs offsets.S gpu940.elf bench940.elf

endif
