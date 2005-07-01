AM_CPPFLAGS=-I$(top_srcdir)/src/include -I$(top_srcdir)/libmissing -I@PGSQL_INCLUDEDIR@ @defined@
pgsql_la_LDFLAGS = -module -avoid-version

if HAVE_PGSQL

pgsql_la_SOURCES = pgsql.c
pgsql_la_LIBADD = $(top_builddir)/src/libpreludedb.la -L@PGSQL_LIBDIR@ -lpq
pgsqldir = $(sql_plugin_dir)

pgsql_LTLIBRARIES = pgsql.la

endif
