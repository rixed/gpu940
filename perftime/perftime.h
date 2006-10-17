#ifndef PERFTIME_H_060504
#define PERFTIME_H_060504

#define PERFTIME_TARGET_MAX 64

#include <stdbool.h>

#define PERF_OTHER 0

struct perftime_stat {
	char const *name;
	unsigned nb_enter;
	unsigned load_avg;	// between 0 and 1024
};

int perftime_begin(void);

// call this frequently and asynchronously with your code
void perftime_async_upd(void);

// name can be NULL
// target 0 is reserved for undef sections.
// set is_enter to false if you return to an interrupted target
void perftime_enter(unsigned target, char const *name, bool is_enter);

unsigned perftime_target(void);

void perftime_reset(void);

void perftime_stat(unsigned target, struct perftime_stat *stat);

void perftime_stat_print(int fd, unsigned target);

void perftime_stat_print_all(int fd);

void perftime_end(void);

#endif
