DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

ACLOCAL_AMFLAGS = -I m4
SUBDIRS = m4 src plugins bindings docs

bin_SCRIPTS = $(top_builddir)/libpreludedb-config $(top_builddir)/prelude-db-create.sh

distuninstallcheck_listfiles = find . -type f -print | grep -v 'perl' | grep -v '3pm'
