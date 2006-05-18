#include <stdlib.h>
#include "gcc.h"
#include "perftime.h"

enum {
	MEMREAD,
	MEMWRITE,
	COMPUTE,
};
#define SIZE 5000

static void compute(void) {
	perftime_enter(COMPUTE, "computing");
	for (unsigned i=0; i<SIZE; i++) {
		volatile unsigned __unused j = (i+1) * 13 / (7 + i);
	}
	perftime_leave();
}

static void mem_read(void) {
	unsigned *c = malloc(SIZE);
	if (! c) return;
	perftime_enter(MEMREAD, "reading mem");
	for (unsigned b=0; b<SIZE; b++) {
		unsigned __unused i = c[b];
	}
	perftime_leave();
	free(c);
}

static void mem_write(void) {
	int *c = malloc(SIZE);
	if (! c) return;
	perftime_enter(MEMWRITE, "writing mem");
	for (unsigned b=0; b<SIZE; b++) {
		c[b] = b;
	}
	perftime_leave();
	free(c);
}

int main(void) {
	if (0 != perftime_begin(0, NULL, 0)) {	// uses default gettimeofday
		return EXIT_FAILURE;
	}
	for (unsigned i=0; i<100; i++) {
		mem_read();
		mem_write();
		compute();
	}
	perftime_stat_print_all(1);	// print all on stdout
	perftime_end();
}
