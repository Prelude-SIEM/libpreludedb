if HAVE_EASY_BINDINGS

SUBDIRS = include

lib_LTLIBRARIES = libpreludedbcpp.la
libpreludedbcpp_ladir = ${prefix}/lib
libpreludedbcpp_la_SOURCES = preludedb.cxx preludedb-path-selection.cxx preludedb-sql.cxx preludedb-error.cxx

libpreludedbcpp_la_LDFLAGS = -no-undefined -version-info @LIBPRELUDEDBCPP_SONAME@
libpreludedbcpp_la_LIBADD  = $(top_builddir)/src/.libs/libpreludedb.la
libpreludedbcpp_la_CPPFLAGS = -I$(top_builddir) -I$(top_builddir)/src/include -I$(top_srcdir)/src/include -I$(top_builddir)/bindings/c++/include -I$(top_srcdir)/bindings/c++/include @LIBPRELUDE_CFLAGS@

endif

-include $(top_srcdir)/git.mk
