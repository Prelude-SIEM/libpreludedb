SUBDIRS = include

AM_CPPFLAGS = @PCFLAGS@ -I$(srcdir)/include -I$(top_srcdir)/libmissing -I$(top_builddir)/libmissing -DSQL_PLUGIN_DIR=\"@sql_plugin_dir@\" -DFORMAT_PLUGIN_DIR=\"@format_plugin_dir@\" @LIBPRELUDE_CFLAGS@

lib_LTLIBRARIES = libpreludedb.la

if HAVE_MYSQL
 MYSQL_OBJS=-dlopen $(top_builddir)/plugins/sql/mysql/mysql.la
endif

if HAVE_PGSQL
 PGSQL_OBJS=-dlopen $(top_builddir)/plugins/sql/pgsql/pgsql.la
endif

if HAVE_SQLITE3
 SQLITE3_OBJS=-dlopen $(top_builddir)/plugins/sql/sqlite3/sqlite3.la
endif

DLOPENED_OBJS=$(MYSQL_OBJS) $(PGSQL_OBJS) $(SQLITE3_OBJS) -dlopen $(top_builddir)/plugins/format/classic/classic.la

libpreludedb_la_LDFLAGS =  \
			  -no-undefined \
			  -version-info @LIBPRELUDEDB_SONAME@ \
			  -export-symbols-regex "^preludedb_" \
			  $(LTLIBTHREAD) \
			  #$(DLOPENED_OBJS)

libpreludedb_la_LIBADD = @LIBPRELUDE_LIBS@ $(LTLIBTHREAD) ../libmissing/libmissing.la

AM_YFLAGS = -d
AM_LFLAGS = --header-file=preludedb-path-selection-parser.lex.h
LEX_OUTPUT_ROOT = lex._preludedbyy

noinst_HEADERS = preludedb-path-selection-parser.lex.h
DISTCLEANFILES = preludedb-path-selection-parser.lex.h preludedb-path-selection-parser.lex.c preludedb-path-selection-parser.yac.h preludedb-path-selection-parser.yac.c


libpreludedb_la_SOURCES =		\
	preludedb.c			\
	preludedb-path-selection.c	\
	preludedb-path-selection-parser.lex.l \
	preludedb-path-selection-parser.yac.y \
	preludedb-plugin-format.c	\
	preludedb-plugin-sql.c		\
	preludedb-sql.c			\
	preludedb-sql-select.c		\
	preludedb-sql-settings.c	\
	preludedb-version.c		\
	preludedb-error.c		

-include $(top_srcdir)/git.mk
