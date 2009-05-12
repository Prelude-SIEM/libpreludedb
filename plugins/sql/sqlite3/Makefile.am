AM_CPPFLAGS=@PCFLAGS@ -I$(top_srcdir)/src/include -I$(top_srcdir)/libmissing -I$(top_builddir)/libmissing @LIBPRELUDE_CFLAGS@ @SQLITE3_CFLAGS@
sqlite3_la_LDFLAGS = -module -avoid-version @LIBPRELUDE_LDFLAGS@

if HAVE_SQLITE3

sqlite3_la_SOURCES = sqlite3.c
sqlite3_la_LIBADD = $(top_builddir)/src/libpreludedb.la @LIBPRELUDE_LIBS@ @SQLITE3_LIBS@
sqlite3dir = $(sql_plugin_dir)
sqlite3_LTLIBRARIES = sqlite3.la

endif
