SUBDIRS = include
DEFS = -I$(top_srcdir) -I$(srcdir)/include @defined@ -DSQL_PLUGIN_DIR=\"@sql_plugin_dir@\" -DFORMAT_PLUGIN_DIR=\"@format_plugin_dir@\"

lib_LTLIBRARIES = libpreludedb.la

libpreludedb_la_LIBADD = @LIBPRELUDE_LIBS@

libpreludedb_la_SOURCES =		\
	gdb.c				\
	strbuf.c			\
	db.c				\
	db-interface.c			\
	db-interface-string.c		\
	db-connection.c			\
	db-connection-data.c		\
	db-object-selection.c		\
	sql-connection-data.c		\
	sql.c				\
	param-string.c			\
	format-plugins.c		\
	sql-plugins.c
