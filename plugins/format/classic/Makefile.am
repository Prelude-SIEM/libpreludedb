SUBDIRS=include
EXTRA_DIST = mysql.sql pgsql.sql schema.txt

AM_CPPFLAGS=-I$(top_srcdir)/src/include -I$(srcdir)/include -DFORMAT_CONFIG_DIR=\"@format_config_dir@\" -DFORMAT_PLUGIN_DIR=\"@format_plugin_dir@\" @defined@

classic_la_LIBADD  = $(top_builddir)/src/libpreludedb.la @LIBPRELUDE_LIBS@
classic_la_LDFLAGS = -module -avoid-version @LIBPRELUDE_LDFLAGS@
classic_la_SOURCES = classic.c idmef-db-insert.c idmef-db-select.c idmef-db-get.c idmef-db-delete.c db-path.c
classic_LTLIBRARIES = classic.la
classicdir = $(format_plugin_dir)

install-data-local:
	$(INSTALL) -d -m 755 $(DESTDIR)/$(format_config_dir)/classic
	$(INSTALL) -m 644 $(srcdir)/*.sql $(DESTDIR)/$(format_config_dir)/classic
	$(INSTALL) -m 644 $(srcdir)/schema.txt $(DESTDIR)/$(format_config_dir)/classic

uninstall-local:
	rm -f $(DESTDIR)/$(format_config_dir)/classic/*.sql
	rm -f $(DESTDIR)/$(format_config_dir)/classic/schema.txt
