AM_CPPFLAGS=-I$(top_srcdir)/src/include -I$(top_srcdir)/libmissing @defined@ @SQLITE3_CFLAGS@
sqlite3_la_LDFLAGS = -module -avoid-version

if HAVE_SQLITE3

sqlite3_la_SOURCES = sqlite3.c
sqlite3_la_LIBADD = $(top_builddir)/src/libpreludedb.la @SQLITE3_LIBS@
sqlite3dir = $(sql_plugin_dir)
sqlite3_LTLIBRARIES = sqlite3.la

endif
