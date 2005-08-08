ACLOCAL_AMFLAGS = -I m4 -I libmissing/m4
DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

SUBDIRS = m4 libmissing src plugins bindings docs

bin_SCRIPTS = $(top_builddir)/libpreludedb-config
distuninstallcheck_listfiles = find . -type f -print | grep -v 'perl' | grep -v '3pm'

bin_PROGRAMS = preludedb-admin
preludedb_admin_LDADD = $(top_builddir)/src/libpreludedb.la
preludedb_admin_SOURCES = preludedb-admin.c
