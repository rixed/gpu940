#include <limits.h>
#include <signal.h>
#include <stdint.h>
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
	unsigned nb_in;
} stats[PERFTIME_TARGET_MAX];

static sig_atomic_t in_target;
static unsigned nb_in_tot;

#ifndef GP2X
static char str[1024];
#endif

/*
 * Private Functions
 */

/*
 * Public Functions
 */

void perftime_reset(void)
{
	in_target = 0;
	nb_in_tot = 0;
	for (unsigned t=0; t<sizeof_array(stats); t++) {
		stats[t].nb_enter = 0;
		stats[t].nb_in = 0;
	}
}

int perftime_begin(void)
{
	perftime_reset();
	stats[0].name = "Undef";
	for (unsigned t=1; t<sizeof_array(stats); t++) {	// reset keeps the names, just flush stats
		stats[t].name = NULL;
	}
	return 0;
}

void perftime_async_upd(void)
{
	stats[in_target].nb_in ++;
	if (++ nb_in_tot == UINT_MAX) {
		// rescale all entries
		for (unsigned t=0; t<sizeof_array(stats); t++) {
			stats[t].nb_in >>= 1;
		}
		nb_in_tot >>= 1;
	}
}

void perftime_enter(unsigned target, char const *name)
{
	in_target = target;
	if (name) {
		stats[target].name = name;
		stats[target].nb_enter ++;
	}
}

unsigned perftime_target(void) {
	return in_target;
}

void perftime_stat(unsigned target, struct perftime_stat *s)
{
	assert(target<sizeof_array(stats));
	s->name = stats[target].name;
	s->nb_enter = stats[target].nb_enter;
	s->load_avg = ((uint64_t)stats[target].nb_in<<10) / nb_in_tot;
}

void perftime_stat_print(int fd, unsigned target)
{
#ifdef GP2X
	(void)fd, (void)target;
#else
	struct perftime_stat s;
	perftime_stat(target, &s);
	snprintf(str, sizeof(str), "PerfStat for %u(%s): %u enters, %.2f load avg\n",
		target,
		s.name ? s.name:"NULL",
		s.nb_enter,
		s.load_avg/1024.
	);
	write(fd, str, strlen(str));
#endif
}

void perftime_stat_print_all(int fd)
{
#ifdef GP2X
	(void)fd;
#else
	for (unsigned t=0; t<sizeof_array(stats); t++) {
		if (stats[t].nb_enter > 0) {
			perftime_stat_print(fd, t);
		}
	}
	write(fd, str, strlen(str)); 
#endif
}

void perftime_end(void)
{
}

