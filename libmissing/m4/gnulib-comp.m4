# Copyright (C) 2004 Free Software Foundation, Inc.
# This file is free software, distributed under the terms of the GNU
# General Public License.  As a special exception to the GNU General
# Public License, this file may be distributed as part of a program
# that contains a configuration script generated by Autoconf, under
# the same distribution terms as the rest of that program.
#
# Generated by gnulib-tool.
#
# This file represents the compiled summary of the specification in
# gnulib-cache.m4. It lists the computed macro invocations that need
# to be invoked from configure.ac.
# In projects using CVS, this file can be treated like other built files.


# This macro should be invoked from ./configure.in, in the section
# "Checks for programs", right after AC_PROG_CC, and certainly before
# any checks for libraries, header files, types and library functions.
AC_DEFUN([gl_EARLY],
[
  AC_REQUIRE([AC_GNU_SOURCE])
  AC_REQUIRE([gl_USE_SYSTEM_EXTENSIONS])
])

# This macro should be invoked from ./configure.in, in the section
# "Check for header files, types and library functions".
AC_DEFUN([gl_INIT],
[
  gl_FUNC_ALLOCA
  dnl gl_USE_SYSTEM_EXTENSIONS must be added quite early to configure.ac.
  gl_MINMAX
  gl_C_RESTRICT
  gl_SIZE_MAX
  gl_FUNC_SNPRINTF
  gl_FUNC_STRDUP
  gl_FUNC_STRNDUP
  gl_FUNC_STRNLEN
  gl_TIME_R
  gl_FUNC_VASNPRINTF
  gl_FUNC_VSNPRINTF
  gl_XSIZE
])

# This macro records the list of files which have been installed by
# gnulib-tool and may be removed by future gnulib-tool invocations.
AC_DEFUN([gl_FILE_LIST], [
  lib/alloca_.h
  lib/asnprintf.c
  lib/minmax.h
  lib/printf-args.c
  lib/printf-args.h
  lib/printf-parse.c
  lib/printf-parse.h
  lib/size_max.h
  lib/snprintf.c
  lib/snprintf.h
  lib/strdup.c
  lib/strdup.h
  lib/strndup.c
  lib/strndup.h
  lib/strnlen.c
  lib/strnlen.h
  lib/time_r.c
  lib/time_r.h
  lib/vasnprintf.c
  lib/vasnprintf.h
  lib/vsnprintf.c
  lib/vsnprintf.h
  lib/xsize.h
  m4/alloca.m4
  m4/eoverflow.m4
  m4/extensions.m4
  m4/intmax_t.m4
  m4/inttypes_h.m4
  m4/longdouble.m4
  m4/longlong.m4
  m4/minmax.m4
  m4/onceonly_2_57.m4
  m4/restrict.m4
  m4/signed.m4
  m4/size_max.m4
  m4/snprintf.m4
  m4/stdint_h.m4
  m4/strdup.m4
  m4/strndup.m4
  m4/strnlen.m4
  m4/time_r.m4
  m4/vasnprintf.m4
  m4/vsnprintf.m4
  m4/wchar_t.m4
  m4/wint_t.m4
  m4/xsize.m4
])