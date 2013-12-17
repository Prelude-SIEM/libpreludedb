if HAVE_PYTHON

EXTRA_DIST = libpreludedb-python.i preludedb.py _preludedb.c

all-am: python-build

python-build: _preludedb.c
	CC="$(BINDINGS_CC)" $(PYTHON) setup.py build

install-exec-hook:
	$(PYTHON) setup.py install `test -n "$(DESTDIR)" && echo --root $(DESTDIR)`

uninstall-hook: 
	$(PYTHON) setup.py uninstall `test -n "$(DESTDIR)" && echo --root $(DESTDIR)`

clean-local:
	$(PYTHON) setup.py clean -a

_preludedb.c: $(top_srcdir)/bindings/libpreludedb.i libpreludedb-python.i
if HAVE_SWIG
	$(SWIG) -I$(top_srcdir)/src/include @LIBPRELUDE_CFLAGS@ -o $@ -python -noproxy -interface _preludedb -module _preludedb $(top_srcdir)/bindings/libpreludedb.i
endif

endif

-include $(top_srcdir)/git.mk
