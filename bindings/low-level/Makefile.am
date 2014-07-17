BINDING_DEPENDS=libpreludedb.i python/libpreludedb-python.i \
		$(top_srcdir)/src/include/preludedb-error.h \
		$(top_srcdir)/src/include/preludedb.h \
		$(top_srcdir)/src/include/preludedb-path-selection.h \
		$(top_srcdir)/src/include/preludedb-plugin-format.h \
		$(top_srcdir)/src/include/preludedb-plugin-sql.h \
		$(top_srcdir)/src/include/preludedb-sql.h \
		$(top_srcdir)/src/include/preludedb-sql-settings.h

SUBDIRS = python

EXTRA_DIST = libpreludedb.i python/setup.py.in
CLEANFILES = python/build
MAINTAINERCLEANFILES =  python/_preludedbold.c

-include $(top_srcdir)/git.mk
