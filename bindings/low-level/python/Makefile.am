if HAVE_PYTHON2

EXTRA_DIST = libpreludedb-python.i preludedbold.py _preludedbold.c

all-am: python-build

python-build: _preludedbold.c
	CC="$(BINDINGS_CC)" $(PYTHON2) setup.py build

install-exec-hook:
	$(PYTHON2) setup.py install `test -n "$(DESTDIR)" && echo --root $(DESTDIR)`

uninstall-hook: 
	$(PYTHON2) setup.py uninstall `test -n "$(DESTDIR)" && echo --root $(DESTDIR)`

clean-local:
	$(PYTHON2) setup.py clean -a

_preludedbold.c: $(top_srcdir)/bindings/low-level/libpreludedb.i libpreludedb-python.i
if HAVE_SWIG
# Force -I/usr/include because some distribution like CentOS doesn't set it by default for swig
	$(SWIG) -I/usr/include -I$(top_srcdir)/src/include @LIBPRELUDE_CFLAGS@ -o $@ -python -noproxy -interface _preludedbold -module _preludedbold $(top_srcdir)/bindings/low-level/libpreludedb.i
endif

endif

-include $(top_srcdir)/git.mk
