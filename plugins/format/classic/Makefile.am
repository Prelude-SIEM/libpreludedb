DEFS=-I$(top_srcdir) -I$(top_srcdir)/src/include -I./include @defined@

classic_la_LDFLAGS = -module -avoid-version
classic_la_SOURCES = classic.c idmef-db-output.c idmef-db-read.c
classic_la_LIBADD = @LIBPRELUDE_LIBS@
classic_LTLIBRARIES = classic.la
classicdir = $(format_plugin_dir)
