SUBDIRS=include

AM_CPPFLAGS=-I$(top_srcdir)/src/include -I$(srcdir)/include -DFORMAT_CONFIG_DIR=\"@format_config_dir@\" -DFORMAT_PLUGIN_DIR=\"@format_plugin_dir@\" @defined@

classic_la_LDFLAGS = -module -avoid-version -L$(top_builddir)/src
classic_la_SOURCES = classic.c idmef-db-insert.c idmef-db-select.c idmef-db-get.c idmef-db-delete.c db-object.c
classic_LTLIBRARIES = classic.la
classicdir = $(format_plugin_dir)

install-data-local:
	$(INSTALL) -d -m 755 $(DESTDIR)/$(format_config_dir)/classic
	$(INSTALL) -m 644 $(srcdir)/*.sql $(DESTDIR)/$(format_config_dir)/classic
	$(INSTALL) -m 644 $(srcdir)/schema.txt $(DESTDIR)/$(format_config_dir)/classic

