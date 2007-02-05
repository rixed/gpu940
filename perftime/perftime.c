#include <limits.h>
#include <stdint.h>
#include "gcc.h"
#include "perftime.h"
#include <assert.h>
#ifndef GP2X
#	include <stdio.h>
#	include <sys/time.h>
#	include <time.h>
#	include <unistd.h>
#	include <string.h>
#else
#	include "../bin/gpu940i.h"
#endif

#ifndef NDEBUG

/*
 * Data Definitions
 */

#undef sizeof_array
#define sizeof_array(x) (sizeof(x)/sizeof(*x))
struct perftime_stats perftime_stats[PERFTIME_TARGET_MAX];

sig_atomic_t perftime_in_target;
static unsigned nb_in_tot;

#ifndef GP2X
static char str[1024];
#endif

/*
 * Private Functions
 */

static inline void perftime_enter(unsigned target, char const *name);
static inline unsigned perftime_target(void);

/*
 * Public Functions
 */

void perftime_reset(void)
{
	perftime_in_target = 0;
	nb_in_tot = 0;
	for (unsigned t=0; t<sizeof_array(perftime_stats); t++) {
		perftime_stats[t].nb_enter = 0;
		perftime_stats[t].nb_in = 0;
	}
}

int perftime_begin(void)
{
	perftime_reset();
	perftime_stats[0].name = "Undef";
	for (unsigned t=1; t<sizeof_array(perftime_stats); t++) {	// reset keeps the names, just flush stats
		perftime_stats[t].name = NULL;
	}
	return 0;
}

void perftime_async_upd(void)
{
	perftime_stats[perftime_in_target].nb_in ++;
	if (++ nb_in_tot == UINT_MAX) {
		// rescale all entries
		for (unsigned t=0; t<sizeof_array(perftime_stats); t++) {
			perftime_stats[t].nb_in >>= 1;
		}
		nb_in_tot >>= 1;
	}
}

void perftime_stat(unsigned target, struct perftime_stat *s)
{
	assert(target<sizeof_array(perftime_stats));
	s->name = perftime_stats[target].name;
	s->nb_enter = perftime_stats[target].nb_enter;
	s->load_avg = nb_in_tot > 0 ? ((uint64_t)perftime_stats[target].nb_in<<10) / nb_in_tot : 0;
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
	for (unsigned t=0; t<sizeof_array(perftime_stats); t++) {
		if (perftime_stats[t].nb_enter > 0) {
			perftime_stat_print(fd, t);
		}
	}
	write(fd, str, strlen(str)); 
#endif
}

void perftime_end(void)
{
}

#endif
