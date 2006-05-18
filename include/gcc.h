#ifndef GCC_H_060219
#define GCC_H_060219
/* From Robert Love's 'http://rlove.org/log/2005102601'
 */
#if __GNUC__ >= 3
# define inline		inline __attribute__ ((always_inline))
/* A "pure" function is one that has no effects except its return value and its
 * return value is a function of only the function's parameters or non-volatile
 * global variables. Any parameter or global variable access must be read-only.
 * Loop optimization and subexpression elimination can be applied to such
 * functions. A common example is strlen(): Given identical inputs, the
 * function's return value (its only effect) is invariant across multiple
 * invocations and thus can be pulled out of a loop and called but once.
 */
# define __pure		__attribute__ ((pure))
/* A "const" function is a stricter variant of a pure function: Such functions
 * cannot access global variables and no parameters may be pointers. Thus their
 * return value is a function of nothing but their passed-by-value parameters.
 * Additional optimizations are possible for such functions. Math functions,
 * such as abs(), are examples of const functions (presuming they don't save
 * state or otherwise pull tricks in the name of optimization). It makes no
 * sense for a const or pure function to return void.
 */
# define __const	__attribute__ ((const))
/* If a function never returns (perhaps because it calls exit()), it can be
 * marked as such and GCC can optimize the call site without regard to the
 * potentiality of the function actually returning. It makes no sense for such
 * a function to have a return value other than void.
 */
# define __noreturn	__attribute__ ((noreturn))
/* If a function returns pointers that can never alias any other data (almost
 * assuredly because it just allocated memory), the function can be marked as
 * such and GCC can improve optimizations.
 */
# define __malloc	__attribute__ ((malloc))
/* This attribute instructs GCC to generate a warning whenever the return value
 * from the function is not stored or used in an expression during invocation.
 * This allows functions whose return value is crucial to ensure that the value
 * is always used.
 */
# define __must_check	__attribute__ ((warn_unused_result))
/* This attribute instructs GCC to generate a warning, at the call site,
 * whenever the function is used. This helps developers wean off of deprecated
 * interfaces.
 */
# define __deprecated	__attribute__ ((deprecated))
/* This attribute tells GCC that, despite apparent reality, a function really
 * is used and to always output the assembly for the function. This is useful
 * if the function is only invoked from assembly and GCC is unaware. It also
 * disables warnings due to lack of use.
 */
# define __used		__attribute__ ((used))
/* This attribute tells GCC that the programmer is aware that a given parameter
 * is unused and not to emit warnings to that end. This is useful if compiling
 * with -W or -Wunused but forced to have unused parameters, as is common in
 * event-driven GUI programming.
 */
# define __unused	__attribute__ ((unused))
/* This attribute tells GCC that a type or variable should be packed into
 * memory, using the minimum amount of space possible, potentially disregarding
 * alignment requirements. If specified on a struct or union, all variables
 * therein are so packed. If specified on just a specific variable, only that
 * type is packed. As an example, a structure with a char followed by an int
 * would most likely find the integer aligned to a memory address not
 * immediately following the char (say, three bytes later). The compiler does
 * this by inserting three bytes of unused packing between the two variables. A
 * packed structure lacks this packing, potentially consuming less memory but
 * failing to meet architecture alignment requirements.
 */
# define __packed	__attribute__ ((packed))
/* The likely() and unlikely() directives instruct GCC that the results of the
 * expression contained within are likely true or unlikely true. GCC can then
 * perform block reordering and other optimizations to improve the performance
 * of conditional branches taken based on the results of these expressions.
 * They should only be used if the result is overwhelmingly in one direction;
 * for example, marking error conditions as unlikely()
 */
# define likely(x)	__builtin_expect (!!(x), 1)
# define unlikely(x)	__builtin_expect (!!(x), 0)
#else
# define inline		/* no inline */
# define __pure		/* no pure */
# define __const	/* no const */
# define __noreturn	/* no noreturn */
# define __malloc	/* no malloc */
# define __must_check	/* no warn_unused_result */
# define __deprecated	/* no deprecated */
# define __used		/* no used */
# define __unused	/* no unused */
# define __packed	/* no packed */
# define likely(x)	(x)
# define unlikely(x)	(x)
#endif
#endif
