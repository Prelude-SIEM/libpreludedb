SUBDIRS=include
EXTRA_DIST = mysql.sql pgsql.sql

AM_CPPFLAGS=-I$(top_srcdir)/src/include -I$(srcdir)/include -DFORMAT_CONFIG_DIR=\"@format_config_dir@\" -DFORMAT_PLUGIN_DIR=\"@format_plugin_dir@\" @defined@

classic_la_LIBADD  = $(top_builddir)/src/libpreludedb.la @LIBPRELUDE_LIBS@
classic_la_LDFLAGS = -module -avoid-version @LIBPRELUDE_LDFLAGS@
classic_la_SOURCES = classic.c classic-delete.c classic-get.c classic-insert.c classic-path-resolve.c classic-sql-join.c classic-sql-select.c
classic_LTLIBRARIES = classic.la
classicdir = $(format_plugin_dir)

schemadatadir = $(format_schema_dir)/classic
dist_schemadata_DATA = mysql.sql pgsql.sql
