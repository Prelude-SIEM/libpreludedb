###
# We don't use libtool to compile PreludeDB.so because libtool --mode=install does not
# allow us to install PreludeDB.so in a directory not in ${prefix}/*
###

DEFS=-I$(top_srcdir) -I$(top_srcdir)/src/include @defined@ -D_GNU_SOURCE @PERL_CFLAGS@
CFLAGS=
LINK = $(CC) -o $@

if HAVE_PERL

bin_PROGRAMS = PreludeDB.so

PreludeDB_so_LDFLAGS = -shared -L$(top_srcdir)/src/.libs
PreludeDB_so_SOURCES = PreludeDB.c
PreludeDB_so_LDADD = @LIBPRELUDE_LIBS@ -lpreludedb
noinst_PROGRAMS = PreludeDB.so

install-exec-local:
	$(INSTALL) -d -m 755 $(DESTDIR)$(perl_dir)
	$(INSTALL) -m 644 PreludeDB.pm $(DESTDIR)$(perl_dir)
	$(INSTALL) -d -m 755 $(DESTDIR)$(perl_dir)/auto/PreludeDB
	strip PreludeDB.so && $(INSTALL) -m 644 PreludeDB.so $(DESTDIR)$(perl_dir)/auto/PreludeDB

endif
