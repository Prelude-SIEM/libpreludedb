###
# We don't use libtool to compile Prelude.so because libtool --mode=install does not
# allow us to install Prelude.so in a directory not in ${prefix}/*
###

DEFS=-I$(top_srcdir) -I$(top_srcdir)/src/include @defined@ -D_GNU_SOURCE @PYTHON_CFLAGS@
CFLAGS=
LINK = $(CC) -o $@

if HAVE_PYTHON

bin_PROGRAMS = _preludedb.so

_preludedb_so_LDFLAGS = -shared -L$(top_builddir)/src/.libs
_preludedb_so_SOURCES = _preludedb.c
_preludedb_so_LDADD = @LIBPRELUDE_LIBS@ -lpreludedb
noinst_PROGRAMS = _preludedb.so

endif

install-exec-local:
if HAVE_PYTHON
	$(INSTALL) -d -m 755 $(DESTDIR)/$(python_dir)
	$(INSTALL) -m 644 $(srcdir)/preludedb.py $(DESTDIR)/$(python_dir)
	$(INSTALL) -m 644 _preludedb.so $(DESTDIR)/$(python_dir)
endif
