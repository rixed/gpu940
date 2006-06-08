#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "title.c"
#include "greetz.c"
#include "never.c"

static void write_pic(int p, unsigned char const *pixel_data) {
	printf("static uint8_t pic%d[] = {\n", p);
	for (unsigned y=0; y<256; y++) {
		uint8_t count = 0;
		char last = 0;
		printf("\t");
		for (unsigned x=0; x<255; x++) {
			if (pixel_data[(y*256+x)*3] == 0) {
				if (! last) {
					printf("%u, ", count);
					count = 1;
				} else {
					count ++;
				}
				last = 1;
			} else {
				if (! last) {
					count ++;
				} else {
					printf("%u, ", count);
					count = 1;
				}
				last = 0;
			}
		}
		if (count) {
			printf("%u,", count);
		}
		printf("\n");
	}
	printf("};\n");
}

int main(void) {
	printf("#include <stdint.h>\n\n");
	write_pic(1, title_pic.pixel_data);
	write_pic(2, greetz_pic.pixel_data);
	write_pic(3, never_pic.pixel_data);
	return 0;
}

