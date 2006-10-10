#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(gpu940, 1.0, rixed@happyleptic.org)
AC_CONFIG_SRCDIR([bin/gpu940.c])
AM_INIT_AUTOMAKE
AC_ARG_ENABLE(gp2x,compile specifically for gp2x,[CFLAGS="$CFLAGS -DGP2X"],[
	AC_CHECK_LIB(SDL, SDL_Init,, AC_MSG_ERROR([SDL is required]))
])
AM_CONDITIONAL(GP2X, [echo $CFLAGS|grep '\-DGP2X'>/dev/null])
HOST_CC=gcc
AC_SUBST(HOST_CC)

# Checks for programs.
AM_PROG_LIBTOOL
AC_PROG_CC
AM_PROG_AS
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdint.h stdlib.h unistd.h])
AM_CONFIG_HEADER(config.h)

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_C_RESTRICT
AC_TYPE_SIZE_T
AC_C_VOLATILE

# Checks for library functions.
AC_FUNC_MMAP
AC_CHECK_FUNCS([getpagesize munmap])
###AC_CHECK_LIB(SDL, SDL_Init,, AC_MSG_ERROR([SDL is required]))

AC_CONFIG_FILES([
	Makefile
	bin/Makefile
	lib/Makefile
	sample/Makefile
	include/Makefile
	include/GL/Makefile
	perftime/Makefile
	console/Makefile
	GL/Makefile
])
AC_OUTPUT
