SUBDIRS = include
AM_CPPFLAGS = @defined@ -I$(srcdir)/include -DSQL_PLUGIN_DIR=\"@sql_plugin_dir@\" -DFORMAT_PLUGIN_DIR=\"@format_plugin_dir@\"

lib_LTLIBRARIES = libpreludedb.la

if HAVE_MYSQL
 MYSQL_OBJS=-dlopen $(top_builddir)/plugins/sql/mysql/mysql.la
endif

if HAVE_PGSQL
 PGSQL_OBJS=-dlopen $(top_builddir)/plugins/sql/pgsql/pgsql.la
endif

DLOPENED_OBJS=$(MYSQL_OBJS) $(PGSQL_OBJS) -dlopen $(top_builddir)/plugins/format/classic/classic.la

libpreludedb_la_LDFLAGS = @LIBPRELUDE_LDFLAGS@ -no-undefined #$(DLOPENED_OBJS)
libpreludedb_la_LIBADD = @LIBPRELUDE_LIBS@

libpreludedb_la_SOURCES =		\
	preludedb.c			\
	preludedb-path-selection.c	\
	preludedb-sql.c			\
	preludedb-sql-settings.c	\
	preludedb-version.c		\
	preludedb-error.c
