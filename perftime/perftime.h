#ifndef PERFTIME_H_060504
#define PERFTIME_H_060504

#define PERFTIME_TARGET_MAX 64

struct perftime_stat {
	char const *name;
	unsigned nb_enter;
	unsigned cumul_secs;	// with 10 bits for decimals
	unsigned average;	// between 0 and 1024
};

// if gettime is NULL, will use gettimeofday or internal GP2X timer ifdef GP2X
int perftime_begin(unsigned freq, unsigned (*gettime)(void), unsigned wrap_after);
// name can be NULL
void perftime_enter(unsigned target, char const *name);
// quit all target (enter iddle, if you prefer). This is different from returning to previous target
void perftime_leave(void);

void perftime_stat(unsigned target, struct perftime_stat *stat);

void perftime_stat_print(int fd, unsigned target);

void perftime_stat_print_all(int fd);

void perftime_end(void);

#endif
