AM_CPPFLAGS=@PCFLAGS@ -I$(top_srcdir)/src/include -I$(top_srcdir)/libmissing -I$(top_builddir)/libmissing @LIBPRELUDE_CFLAGS@ @MYSQL_CFLAGS@
mysql_la_LDFLAGS = -module -avoid-version @LIBPRELUDE_LDFLAGS@

if HAVE_MYSQL

mysql_la_SOURCES = mysql.c
mysql_la_LIBADD = $(top_builddir)/src/libpreludedb.la @LIBPRELUDE_LIBS@ @MYSQL_LDFLAGS@
mysqldir = $(sql_plugin_dir)
mysql_LTLIBRARIES = mysql.la

endif

-include $(top_srcdir)/git.mk
