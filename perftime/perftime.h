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

#ifndef NDEBUG

int perftime_begin(void);

// call this frequently and asynchronously with your code
void perftime_async_upd(void);

void perftime_reset(void);

void perftime_stat(unsigned target, struct perftime_stat *stat);

void perftime_stat_print(int fd, unsigned target);

void perftime_stat_print_all(int fd);

void perftime_end(void);

#include <signal.h>
extern sig_atomic_t perftime_in_target;
extern struct perftime_stats {
	char const *name;
	unsigned nb_enter;
	unsigned nb_in;
} perftime_stats[PERFTIME_TARGET_MAX];

// name can be NULL ; in this case, we suppose we are returning to target
// target 0 is reserved for undef sections.
static inline void perftime_enter(unsigned target, char const *name)
{
	perftime_in_target = target;
	if (name) {
		perftime_stats[target].name = name;
		perftime_stats[target].nb_enter ++;
	}
}

static inline unsigned perftime_target(void) {
	return perftime_in_target;
}

#else	// NDEBUG
#	define perftime_begin(x) 0
#	define perftime_async_upd()
#	define perftime_reset()
#	define perftime_stat(target, stat) do { (void)target; (void)stat; } while(0)
#	define perftime_stat_print(fd, target) do { (void)fd; (void)target; } while(0)
#	define perftime_stat_print_all(fd) do { (void)fd; } while(0)
#	define perftime_end(x) do { (void)x; } while(0)
#	define perftime_enter(target, name) do { (void)target; (void)name; } while(0)
#	define perftime_target(x) 0
#endif

#endif
