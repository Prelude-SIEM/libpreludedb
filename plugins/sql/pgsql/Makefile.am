AM_CPPFLAGS=@PCFLAGS@ -I$(top_srcdir)/src/include -I$(top_srcdir)/libmissing -I$(top_builddir)/libmissing @LIBPRELUDE_CFLAGS@ @POSTGRESQL_CFLAGS@
pgsql_la_LDFLAGS = -module -avoid-version @LIBPRELUDE_LDFLAGS@ 

if HAVE_PGSQL

pgsql_la_SOURCES = pgsql.c
pgsql_la_LIBADD = $(top_builddir)/src/libpreludedb.la @LIBPRELUDE_LIBS@ @POSTGRESQL_LDFLAGS@
pgsqldir = $(sql_plugin_dir)

pgsql_LTLIBRARIES = pgsql.la

endif

-include $(top_srcdir)/git.mk
