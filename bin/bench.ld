SECTIONS {
	upper32 0x0000 : {
		bench.o(.text)
	}
	/DISCARD/ : {
		*(*)
	}
}
