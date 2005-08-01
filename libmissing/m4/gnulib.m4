# Copyright (C) 2004 Free Software Foundation, Inc.
# This file is free software, distributed under the terms of the GNU
# General Public License.  As a special exception to the GNU General
# Public License, this file may be distributed as part of a program
# that contains a configuration script generated by Autoconf, under
# the same distribution terms as the rest of that program.
#
# Generated by gnulib-tool.
#
# Invoked as: gnulib-tool --import --dir=. --lib=libmissing --source-base=libmissing --m4-base=libmissing/m4 --libtool strdup strndup snprintf vsnprintf time_r
# Reproduce by: gnulib-tool --import --dir=. --lib=libmissing --source-base=libmissing --m4-base=libmissing/m4 --aux-dir=. --libtool  alloca-opt extensions minmax restrict size_max snprintf strdup strndup strnlen time_r vasnprintf vsnprintf xsize

AC_DEFUN([gl_EARLY],
[
  AC_GNU_SOURCE
  gl_USE_SYSTEM_EXTENSIONS
])

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

dnl Usage: gl_MODULES(module1 module2 ...)
AC_DEFUN([gl_MODULES], [])

dnl Usage: gl_SOURCE_BASE(DIR)
AC_DEFUN([gl_SOURCE_BASE], [])

dnl Usage: gl_M4_BASE(DIR)
AC_DEFUN([gl_M4_BASE], [])

dnl Usage: gl_LIB(LIBNAME)
AC_DEFUN([gl_LIB], [])

dnl Usage: gl_LGPL
AC_DEFUN([gl_LGPL], [])

# gnulib.m4 ends here
