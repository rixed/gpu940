#include <limits.h>
#include "gcc.h"
#include "perftime.h"
#ifndef GP2X
#	include <stdio.h>
#	include <assert.h>
#	include <sys/time.h>
#	include <time.h>
#	include <unistd.h>
#	include <string.h>
#else
#	include "../bin/gpu940i.h"
#	define assert(x)
#endif

/*
 * Data Definitions
 */

#undef sizeof_array
#define sizeof_array(x) (sizeof(x)/sizeof(*x))
static struct {
	char const *name;
	unsigned nb_enter;
	unsigned last_time_in;
	unsigned long long tot_time;
} stats[PERFTIME_TARGET_MAX];

static unsigned (*gettime)(void);
static unsigned freq;
static unsigned wrap_after;
static unsigned in_target;
static unsigned long long total_time;	// if a timer can make an unsigned wrap, then total_time must be greater.

#ifndef GP2X
static char str[1024];
#endif

/*
 * Private Functions
 */

static unsigned my_gettime(void) {
#ifdef GP2X
	return gp2x_regs32[0x0c08>>2];	// this subcounter is fast (1024Hz) and never written by linux (nor writable)
#else
	struct timeval tv;
	if (-1 == gettimeofday(&tv, NULL)) {
		assert(0);
	}
	return (unsigned)tv.tv_usec;
#endif
}

static void update_in_target(unsigned now) {
	// update previous target
	long long unsigned elapsed_time;
	if (likely(now >= stats[in_target].last_time_in)) {	// no wrap
		elapsed_time = now - stats[in_target].last_time_in;
	} else {
		elapsed_time = 0;//now + (wrap_after - stats[in_target].last_time_in + 1);
	}
	stats[in_target].tot_time += elapsed_time;
	total_time += elapsed_time;
}

/*
 * Public Functions
 */

void perftime_reset(void) {
	total_time = 0;
	in_target = ~0U;
	for (unsigned t=0; t<sizeof_array(stats); t++) {
		stats[t].nb_enter = 0;
		stats[t].tot_time = 0;
	}
}

int perftime_begin(unsigned freq_, unsigned (*gettime_)(void), unsigned wrap_after_) {
	perftime_reset();
	for (unsigned t=0; t<sizeof_array(stats); t++) {	// reset keeps the names, just flush stats
		stats[t].name = NULL;
	}
	if (gettime_) {
		gettime = gettime_;
		freq = freq_;
		wrap_after = wrap_after_;
	} else {
		gettime = my_gettime;
#ifdef GP2X
		freq = 1024; //7372800>>16;
		wrap_after = 1023; //0xffff;
#else
		freq = 1000000;
		wrap_after = 999999;
#endif
	}
	if (0 == freq || 0 == wrap_after) return -1;
	return 0;
}

void perftime_enter(unsigned target, char const *name) {
	return;
	unsigned now = gettime();
	if (~0U != in_target) update_in_target(now);
	if (~0U != target) {
		if (name) stats[target].name = name;
		stats[target].nb_enter ++;
		stats[target].last_time_in = now;
	}
	in_target = target;
}

unsigned perftime_target(void) {
	return in_target;
}

void perftime_stat(unsigned target, struct perftime_stat *s) {
	assert(target<sizeof_array(stats));
	assert(~0U != target);
	s->name = stats[target].name;
	s->nb_enter = stats[target].nb_enter;
	s->cumul_secs = (1024ULL * stats[target].tot_time) / freq;
	assert(total_time >= stats[target].tot_time);
	if (total_time) s->average = (100U * stats[target].tot_time) / total_time;
	else s->average = 0;
}

void perftime_stat_print(int fd, unsigned target) {
#ifdef GP2X
	(void)fd, (void)target;
#else
	struct perftime_stat s;
	perftime_stat(target, &s);
	snprintf(str, sizeof(str), "PerfStat for %u(%s): %u enters, %.2f cumul secs, %03.2f%%\n",
		target,
		s.name ? s.name:"unset",
		s.nb_enter,
		s.cumul_secs/1024.,
		100.*s.average/1024.
	);
	write(fd, str, strlen(str));
#endif
}

void perftime_stat_print_all(int fd) {
#ifdef GP2X
	(void)fd;
#else
	for (unsigned t=0; t<sizeof_array(stats); t++) {
		if (stats[t].nb_enter > 0) {
			perftime_stat_print(fd, t);
		}
	}
	snprintf(str, sizeof(str), "Total: %.2f cumul secs\n", (double)total_time / freq );
	write(fd, str, strlen(str)); 
#endif
}

void perftime_end(void) {
}

