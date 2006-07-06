/* OUTPUT_FORMAT(binary) won't work because ld cannot change from arm-coff to binary, for unknown reason. will use objcopy. */
SECTIONS {
	upper32 0x0000 : {
		crt0.o(.text)
		gpu940.o(.text)
		mylib.o(.text)
		clip.o(.text)
		poly.o(.text)
		draw_line.gp2x.o(.text)
		text.o(.text)
		fixmath.o(.text)
		libconsole.a(.text)
		libperftime.a(.text)
		mydiv.o(.text)
		. = ALIGN(16);
		*(.data)
		. = ALIGN(4);
		*(.rodata*)
		. = ALIGN(4);
		*(COMMON)
		. = ALIGN(4);
		*(.bss)
	}
	/DISCARD/ : {
		*(*)
	}
}
