SUBDIRS = include
DEFS = @defined@ -DSQL_PLUGIN_DIR=\"@sql_plugin_dir@\" -DFORMAT_PLUGIN_DIR=\"@format_plugin_dir@\"
INCLUDES = -I$(top_srcdir) -I$(srcdir)/include

lib_LTLIBRARIES = libpreludedb.la

libpreludedb_la_LIBADD = @LIBPRELUDE_LIBS@

libpreludedb_la_SOURCES =		\
	gdb.c				\
	db.c				\
	db-connection.c			\
	format-plugins.c		\
	sql-plugins.c			\
	sql-table.c
	
