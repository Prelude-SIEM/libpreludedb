m4_define([libpreludedb_major_version], [5])
m4_define([libpreludedb_minor_version], [2])
m4_define([libpreludedb_micro_version], [0])
m4_define([libpreludedb_level_version], [LIBPRELUDEDB_RELEASE_LEVEL_FINAL])
m4_define([libpreludedb_serial_version], [0])

m4_define([serial_string], ifelse(libpreludedb_level_version, LIBPRELUDEDB_RELEASE_LEVEL_ALPHA, alpha,
                                  ifelse(libpreludedb_level_version, LIBPRELUDEDB_RELEASE_LEVEL_BETA, beta,
                                  ifelse(libpreludedb_level_version, LIBPRELUDEDB_RELEASE_LEVEL_RC, rc))))

m4_define([version_error], ifelse(serial_string, [], ,
                                  ifelse(libpreludedb_serial_version, 0, error)))
ifelse(version_error, [], , [m4_fatal(Release level 'serial_string' and serial 'libpreludedb_serial_version' are incompatible)])

m4_append([serial_string], ifelse(serial_string, [],
                                  ifelse(libpreludedb_serial_version, 0, , .libpreludedb_serial_version),
                                  libpreludedb_serial_version))

AC_INIT(libpreludedb, m4_join([.], libpreludedb_major_version,
                                   libpreludedb_minor_version,
                                   m4_join([], libpreludedb_micro_version, serial_string)))

LIBPRELUDEDB_MAJOR_VERSION=libpreludedb_major_version
LIBPRELUDEDB_MINOR_VERSION=libpreludedb_minor_version
LIBPRELUDEDB_MICRO_VERSION=libpreludedb_micro_version
LIBPRELUDEDB_LEVEL_VERSION=libpreludedb_level_version
LIBPRELUDEDB_SERIAL_VERSION=libpreludedb_serial_version

dnl **********************************************************
dnl * Library soname (https://www.sourceware.org/autobook/autobook/autobook_61.html#Library-Versioning)
dnl **********************************************************
libpreludedb_current=11
libpreludedb_revision=1
libpreludedb_age=4
LIBPRELUDEDB_SONAME=$libpreludedb_current:$libpreludedb_revision:$libpreludedb_age

libpreludedbcpp_current=6
libpreludedbcpp_revision=1
libpreludedbcpp_age=4
LIBPRELUDEDBCPP_SONAME=$libpreludedbcpp_current:$libpreludedbcpp_revision:$libpreludedbcpp_age

AC_PREREQ(2.59)
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AC_CONFIG_SRCDIR([src])
AM_INIT_AUTOMAKE([-Wall tar-pax 1.9])

AC_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE


dnl **********************************************************
dnl * Check required program.
dnl **********************************************************
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_CC
AC_PROG_CXX
AM_PROG_LEX
AC_PROG_YACC


BINDINGS_CC="$CC"


dnl **********************************************************
dnl * Setup GnuLib, libtool and libltdl.
dnl **********************************************************

dnl Provide replacement for system missing this GnuLib required macro.
m4_ifndef([AS_VAR_COPY], [m4_define([AS_VAR_COPY], 
       [AS_LITERAL_IF([$1[]$2], [$1=$$2], [eval $1=\$$2])])]) 

gl_EARLY
AC_CONFIG_MACRO_DIR([m4])
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

AC_LIBTOOL_DLOPEN
AC_LIBTOOL_WIN32_DLL
AM_DISABLE_STATIC
AC_PROG_LIBTOOL



dnl **************************************************
dnl * Setup custom compiler flags                    *
dnl **************************************************

AC_MSG_CHECKING(for cpu-specific compiler flags)
if test "$host_cpu" == "x86_64" && test $enable_static = "yes"; then
        AC_MSG_RESULT(x86_64/static: adding -fPIC)
        AX_CHECK_COMPILE_FLAG(-fPIC, CPPFLAGS="$CPPFLAGS -fPIC")
else
        AC_MSG_RESULT(none needed)
fi

AC_MSG_CHECKING(for platform-specific compiler flags)
case "$host_os" in
 darwin*)
        # It may be called "cc", but it's really a GCC derivative
        # with a problematic special precompiler and precompiled
        # headers; turn off the special precompiler, as some
        # apparently-legal code won't compile with its precompiled
        # headers.
        AC_MSG_RESULT(Darwin: adding -no-cpp-precomp)
        AX_CHECK_COMPILE_FLAG(-no-cpp-precomp, PCFLAGS="$PCFLAGS -no-cpp-precomp")
        libext=dylib
        ;;
 *)
        libext=so
        AC_MSG_RESULT(none needed)
        ;;
esac

AX_CHECK_LINK_FLAG([-z relro], LDFLAGS="$LDFLAGS -z relro")
AX_CHECK_LINK_FLAG([-z now], LDFLAGS="$LDFLAGS -z now")


for i in -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations \
         -Wbad-function-cast -Wcast-qual -Wcast-align -Wnested-externs -Wunused \
         -Wformat -Wformat-security; do
        AX_CHECK_COMPILE_FLAG([$i], PCFLAGS="$PCFLAGS $i")
done

if test x$buggy_pthread_initializers = xyes; then
   AX_CHECK_COMPILE_FLAG([-Wno-missing-braces], PCFLAGS="$PCFLAGS -Wno-missing-braces")
fi


dnl **************************************************
dnl * Check for libprelude                           *
dnl **************************************************

AM_PATH_LIBPRELUDE(5.2.0, , AC_MSG_ERROR(Cannot find libprelude: Is libprelude-config in the path?), no)


dnl ***************************************************
dnl * Check for the MySQL library (MySQL plugin       *
dnl ***************************************************
AX_LIB_MYSQL()
with_mysql="no"
if test x$MYSQL_CONFIG != x; then
        LIBS_bkp=$LIBS
        LIBS="$MYSQL_LDFLAGS"
        AC_CHECK_FUNC(mysql_real_escape_string, AC_DEFINE(HAVE_MYSQL_REAL_ESCAPE_STRING, , mysql_real_escape_string function present))
        LIBS=$LIBS_bkp

        CPPFLAGS_bkp=$CPPFLAGS
        CPPFLAGS="$MYSQL_CFLAGS"
        AC_CHECK_HEADER(mysql.h, with_mysql=yes, with_mysql=no)
        CPPFLAGS=$CPPFLAGS_bkp;
fi

AM_CONDITIONAL(HAVE_MYSQL, test x$with_mysql = xyes)


dnl ********************************************************
dnl * Check for the PostgreSQL library (PostgreSQL plugin) *
dnl ********************************************************
AX_LIB_POSTGRESQL()

with_pgsql="no"
if test x$POSTGRESQL_VERSION != x; then
        LIBS_bkp=$LIBS
        LIBS="$POSTGRESQL_LDFLAGS"
        AC_CHECK_FUNC(PQserverVersion, AC_DEFINE(HAVE_PQSERVERVERSION, , [Define if PQserverVersion function is available]))
        AC_CHECK_FUNC(PQescapeStringConn, AC_DEFINE(HAVE_PQESCAPESTRINGCONN, , [Define if PQescapeStringConn function is available]))
        AC_CHECK_FUNC(PQescapeByteaConn, AC_DEFINE(HAVE_PQESCAPEBYTEACONN, , [Define if PQescapeByteaConn function is available]))
        LIBS=$LIBS_bkp;

        CPPFLAGS_bkp=$CPPFLAGS
        CPPFLAGS="$POSTGRESQL_CFLAGS"
        AC_CHECK_HEADER(libpq-fe.h, with_pgsql=yes, with_pgsql=no)
        CPPFLAGS=$CPPFLAGS_bkp;
fi

AM_CONDITIONAL(HAVE_PGSQL, test x$with_pgsql = xyes)

dnl ********************************************************
dnl * Check for the SQLite3 library (SQLite3 plugin)       *
dnl ********************************************************
AX_LIB_SQLITE3("3.0.0")

with_sqlite3="no"
if test x$SQLITE3_VERSION != x; then
        CPPFLAGS_bkp=$CPPFLAGS
        CPPFLAGS="$SQLITE3_CFLAGS"
        AC_CHECK_HEADER(sqlite3.h, with_sqlite3=yes, with_sqlite3=no)
        CPPFLAGS=$CPPFLAGS_bkp;
fi

AM_CONDITIONAL(HAVE_SQLITE3, test x$with_sqlite3 = xyes)



dnl **************************************************
dnl * Swig support                                   *
dnl **************************************************

AC_ARG_WITH(swig, AC_HELP_STRING(--with-swig@<:@=PATH@:>@, Re-generate python bindings sources @<:@default=auto@:>@),
            [swig_required=true; if test x$withval = xyes; then with_swig="swig"; fi], with_swig="no")

SWIG="no"
if test x$with_swig != xno; then
   AC_PATH_PROG(SWIG, `basename $with_swig`, no, `dirname $with_swig`:$PATH)

   if test x$SWIG = xno; then
      if test x$swig_required = xtrue; then
         AC_MSG_ERROR([Could not find $with_swig binary])
      fi
   fi
fi

AM_CONDITIONAL(HAVE_SWIG, test x$SWIG != xno)



dnl **************************************************
dnl * Python support                                 *
dnl **************************************************

AC_ARG_WITH(python2, AC_HELP_STRING(--with-python2@<:@=PATH@:>@, Enable support for python2 binding @<:@default=auto@:>@),
            [python2_required=true; if test x$withval = xyes; then with_python2="python2"; fi], with_python2="python2")

if test x$with_python2 != xno; then
   AC_PATH_PROGS(PYTHON2, $with_python2 "python2.7" "python2.6" "python2.5" "python2.4" "python2.3" "python2.2" "python2.1" "python2.0", no, `dirname $with_python2`:$PATH)
   if test x$PYTHON2 = xno; then
      if test x$python2_required = xtrue; then
         AC_MSG_ERROR([Could not find $with_python2 binary])
      fi

      with_python2=no
   else
      old_CPPFLAGS="$CPPFLAGS"
      CPPFLAGS="-I`$PYTHON2 -c 'from distutils.sysconfig import get_python_inc; print(get_python_inc())'`"

      AC_CHECK_HEADER(Python.h, with_python2=yes, with_python2=no)
      CPPFLAGS="$old_CPPFLAGS"
      AX_RESET_HEADERS_CACHE([Python.h])
   fi
fi

AM_CONDITIONAL(HAVE_PYTHON2, test x$with_python2 = xyes)


AC_ARG_WITH(python3, AC_HELP_STRING(--with-python3@<:@=PATH@:>@, Enable support for python3 binding @<:@default=auto@:>@),
            [python3_required=true; if test x$withval = xyes; then with_python3="python3"; fi], with_python3="python3")

if test x$with_python3 != xno; then
   AC_PATH_PROGS(PYTHON3, $with_python3 "python3.8" "python3.7" "python3.6" "python3.5" "python3.4" "python3.3" "python3.2" "python3.1" "python3.0", no, `dirname $with_python3`:$PATH)
   if test x$PYTHON3 = xno; then
      if test x$python3_required = xtrue; then
         AC_MSG_ERROR([Could not find $with_python3 binary])
      fi

      with_python3=no
   else
      old_CPPFLAGS="$CPPFLAGS"
      CPPFLAGS="-I`$PYTHON3 -c 'from distutils.sysconfig import get_python_inc; print(get_python_inc())'`"

      AC_CHECK_HEADER(Python.h, with_python3=yes, with_python3=no)
      CPPFLAGS="$old_CPPFLAGS"
   fi
fi

AM_CONDITIONAL(HAVE_PYTHON3, test x$with_python3 = xyes)
AM_CONDITIONAL(HAVE_PYTHON, test x$with_python2 = xyes -o x$with_python3 = xyes)



dnl **************************************************
dnl * Easy bindings support                          *
dnl **************************************************

AC_ARG_ENABLE(easy-bindings, AC_HELP_STRING(--enable-easy-bindings, Enable support for high level binding @<:@default=yes@:>@),
                enable_easy_bindings=$enableval, enable_easy_bindings="yes")

AM_CONDITIONAL(HAVE_EASY_BINDINGS, test x$enable_easy_bindings = xyes)

if test x$enable_easy_bindings = xyes; then
        AC_CHECK_PROG(HAVE_CXX, $CXX, yes, no)
        if test x$HAVE_CXX != xyes; then
                AC_MSG_ERROR([Easybindings require a C++ compiler, but none were found.])
        fi
fi


dnl **************************************************
dnl * Check for missing function replacement         *
dnl **************************************************
gl_SOURCE_BASE(libmissing)
gl_M4_BASE(libmissing/m4)
gl_INIT


dnl **************************************************
dnl * Check for GTK-DOC                              *
dnl **************************************************

GTK_DOC_CHECK(1.0)


dnl **************************************************
dnl * Typedefs, structures, compiler characteristics.*
dnl **************************************************

AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_HEADER_TIME


dnl **************************************************
dnl * Setup build variable                           *
dnl **************************************************

AC_SUBST(PCFLAGS)
AC_SUBST(BINDINGS_CC)
AC_SUBST(LIBPRELUDEDB_SONAME)
AC_SUBST(LIBPRELUDEDBCPP_SONAME)

AS_AC_EXPAND(LIBDIR, $libdir)
AS_AC_EXPAND(DATADIR, $datadir)
AS_AC_EXPAND(SYSCONFDIR, $sysconfdir)


db_plugin_dir=$LIBDIR/libpreludedb/plugins
sql_plugin_dir=$db_plugin_dir/sql
format_plugin_dir=$db_plugin_dir/formats
format_schema_dir=$DATADIR/libpreludedb

AC_SUBST(db_plugin_dir)
AC_SUBST(sql_plugin_dir)
AC_SUBST(format_plugin_dir)
AC_SUBST(format_schema_dir)
AC_SUBST(LIBPRELUDEDB_VERSION)
AC_SUBST(LIBPRELUDEDB_MAJOR_VERSION)
AC_SUBST(LIBPRELUDEDB_MINOR_VERSION)
AC_SUBST(LIBPRELUDEDB_MICRO_VERSION)
AC_SUBST(LIBPRELUDEDB_LEVEL_VERSION)
AC_SUBST(LIBPRELUDEDB_SERIAL_VERSION)


AC_CONFIG_FILES([
libpreludedb-config

Makefile
m4/Makefile
libmissing/Makefile
libmissing/tests/Makefile

src/Makefile
src/include/Makefile
src/include/preludedb-version.h

plugins/Makefile

plugins/sql/Makefile
plugins/sql/mysql/Makefile
plugins/sql/pgsql/Makefile
plugins/sql/sqlite3/Makefile

plugins/format/Makefile
plugins/format/classic/Makefile
plugins/format/classic/include/Makefile

docs/Makefile
docs/api/Makefile
docs/manpages/Makefile

bindings/Makefile
bindings/c++/Makefile
bindings/c++/include/Makefile
bindings/python/Makefile
bindings/python/setup.py
])
AC_CONFIG_COMMANDS([default],[[ chmod +x libpreludedb-config ]],[[]])
AC_OUTPUT


echo
echo "*** Dumping configuration ***"
echo "    - Generate documentation      : $enable_gtk_doc"
echo "    - Enable MySQL plugin         : $with_mysql"
echo "    - Enable PostgreSQL plugin    : $with_pgsql"
echo "    - Enable SQLite3 plugin       : $with_sqlite3"
echo "    - Python2.x binding           : $with_python2";
echo "    - Python3.x binding           : $with_python3";
echo "    - Easy bindings               : $enable_easy_bindings"
