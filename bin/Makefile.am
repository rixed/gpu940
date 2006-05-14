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
	draw_line.gp2x.S \
	crt0.S \
	mydiv.c
#AM_CFLAGS += -fno-pic -ffreestanding
load940_SOURCES = load940.c

if GP2X
HOST_CC=gcc

# Trick to use the context structure fomr within .S files
writeoffs: writeoffs.c gpu940i.h ../include/gpu940.h
#	$(CC) $(CFLAGS) $(AM_CFLAGS) $(LDFLAGS) $(AM_LDFLAGS) -std=c99 -Wall -W -o writeoffs writeoffs.c
	$(HOST_CC) -std=c99 -Wall -W -o $@ $< -I$(top_srcdir)/include
offsets.S: writeoffs
	$< > $@
draw_line.gp2x.S: offsets.S

OBJCOPY = /usr/local/devkitPro/devkitGP2X/bin/arm-linux-objcopy
OBJDUMP = /usr/local/devkitPro/devkitGP2X/bin/arm-linux-objdump
gpu940: $(gpu940_OBJECTS) script.ld
	$(LD) -o $@.elf $(LDFLAGS) -T script.ld -N -s
	$(OBJCOPY) --output-target binary --only-section upper32 $@.elf $@

gpu940.source: gpu940
	$(OBJDUMP) --target=binary --architecture=arm -D $< > $@

clean-local:
	rm -f writeoffs offsets.S gpu940.elf

endif

gpu940_LDADD = -lperftime

