SUBDIRS=include

AM_CPPFLAGS=-I$(top_srcdir)/src/include -I$(srcdir)/include -I$(top_srcdir)/libmissing -I$(top_builddir)/libmissing -DFORMAT_CONFIG_DIR=\"@format_config_dir@\" -DFORMAT_PLUGIN_DIR=\"@format_plugin_dir@\" @defined@

classic_la_LIBADD  = $(top_builddir)/src/libpreludedb.la @LIBPRELUDE_LIBS@
classic_la_LDFLAGS = -module -avoid-version @LIBPRELUDE_LDFLAGS@
classic_la_SOURCES = classic.c classic-delete.c classic-get.c classic-insert.c classic-path-resolve.c classic-sql-join.c classic-sql-select.c
classic_LTLIBRARIES = classic.la
classicdir = $(format_plugin_dir)

EXTRA_DIST = mysql2sqlite.sh mysql2pgsql.sh



schemadatadir = $(format_schema_dir)/classic
dist_schemadata_DATA =  mysql2sqlite.sh		\
			mysql2pgsql.sh		\
			mysql.sql             	\
			mysql-update-14-1.sql 	\
			mysql-update-14-2.sql 	\
			mysql-update-14-3.sql	\
			mysql-update-14-4.sql	\
			mysql-update-14-5.sql	\
			mysql-update-14-6.sql	\
			mysql-update-14-7.sql   \
			pgsql.sql 		\
			pgsql-update-14-1.sql	\
			pgsql-update-14-2.sql	\
			pgsql-update-14-3.sql	\
			pgsql-update-14-4.sql	\
			pgsql-update-14-5.sql	\
			pgsql-update-14-6.sql	\
			pgsql-update-14-7.sql   \
			sqlite.sql		\
			sqlite-update-14-4.sql	\
			sqlite-update-14-5.sql	\
			sqlite-update-14-6.sql  \
			sqlite-update-14-7.sql


sqlite.sql: mysql.sql
	$(srcdir)/mysql2sqlite.sh $(srcdir)/mysql.sql > $@

pgsql.sql: mysql.sql
	$(srcdir)/mysql2pgsql.sh $(srcdir)/mysql.sql > $@
