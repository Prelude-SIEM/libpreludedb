BINDING_DEPENDS=libpreludedb.i $(top_srcdir)/src/include/db-type.h $(top_srcdir)/src/include/db.h \
	$(top_srcdir)/src/include/db-object-selection.h $(top_srcdir)/src/include/db-message-ident.h \
	$(top_srcdir)/src/include/db-interface.h $(top_srcdir)/src/include/db-interface-string.h \
	$(top_srcdir)/src/include/db-connection.h $(top_srcdir)/src/include/sql.h

EXTRA_DIST = libpreludedb.i perl/Makefile.PL.in perl/PreludeDB.pm perl/PreludeDB.c python/setup.py.in python/preludedb.py python/_preludedb.c

all-am: perl python


if HAVE_PERL
perl: perl/PreludeDB.c perl/Makefile perl-build
else
perl:
endif


if HAVE_PYTHON
python: python/_preludedb.c python-build
else
python:
endif



perl/Makefile: perl/Makefile.PL
	cd perl && DESTDIR=$(DESTDIR) $(PERL) Makefile.PL


# proxy classes are an option for swig <= 1.3.19 and are activated by default with swig > 1.3.19
# as we don't want proxy classes, we must explicitly tell swig > 1.3.19 (that support the -noproxy option)
# to not generate them
perl/PreludeDB.c: $(BINDING_DEPENDS)
if HAVE_SWIG
	$(SWIG) -I$(top_srcdir)/src/include -o $@ -perl5 -nopm \
	 `$(SWIG) -perl5 -help 2>&1 | grep noproxy > /dev/null && echo -noproxy` $<
endif

python/_preludedb.c: $(BINDING_DEPENDS)
if HAVE_SWIG
	$(SWIG) -I$(top_srcdir)/src/include -o $@ -python -interface _preludedb -module _preludedb -noproxy $<
endif


perl-build:
	cd perl && $(MAKE) CC=$(CC)

python-build:
	cd python && $(PYTHON) setup.py build



install-exec-hook:
if HAVE_PERL
	cd perl && DESTDIR=$(DESTDIR) $(PERL) Makefile.PL && $(MAKE) install
endif

if HAVE_PYTHON
	cd python && $(PYTHON) setup.py install `test -n "$(DESTDIR)" && echo --root $(DESTDIR)`
endif

uninstall-hook:
if HAVE_PERL
	cd perl && DESTDIR=$(DESTDIR) prefix=$(prefix) $(PERL) Makefile.PL && $(MAKE) uninstall
endif

if HAVE_PYTHON
	cd python && python setup.py uninstall `test -n "$(DESTDIR)" && echo --root $(DESTDIR)`
endif


clean-am: perl/Makefile
if HAVE_PERL
	cd perl && $(MAKE) clean
endif

if HAVE_PYTHON
	cd python && $(PYTHON) setup.py clean -a
endif
