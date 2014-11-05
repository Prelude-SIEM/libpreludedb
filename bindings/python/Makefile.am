if HAVE_EASY_BINDINGS
if HAVE_PYTHON

EXTRA_DIST = libpreludedbcpp-python.i preludedb.py _preludedb.cxx pystrings.swg

all-am: python-build

python-build: _preludedb.cxx
if HAVE_PYTHON2
	CC="$(CXX)" $(PYTHON2) setup.py build
endif
if HAVE_PYTHON3
	CC="$(CXX)" $(PYTHON3) setup.py build
endif
install-exec-hook:
if HAVE_PYTHON2
	$(PYTHON2) setup.py install `test -n "$(DESTDIR)" && echo --root $(DESTDIR)`
endif
if HAVE_PYTHON3
	$(PYTHON3) setup.py install `test -n "$(DESTDIR)" && echo --root $(DESTDIR)`
endif

uninstall-hook:
if HAVE_PYTHON2
	$(PYTHON2) setup.py uninstall `test -n "$(DESTDIR)" && echo --root $(DESTDIR)`
endif
if HAVE_PYTHON3
	$(PYTHON3) setup.py uninstall `test -n "$(DESTDIR)" && echo --root $(DESTDIR)`
endif

clean-local:
if HAVE_PYTHON2
	$(PYTHON2) setup.py clean -a
endif
if HAVE_PYTHON3
	$(PYTHON3) setup.py clean -a
endif


_preludedb.cxx preludedb.py: $(top_srcdir)/bindings/c++/include/*.hxx $(top_srcdir)/bindings/libpreludedbcpp.i libpreludedbcpp-python.i
if HAVE_SWIG
	$(SWIG) -c++ @LIBPRELUDE_CFLAGS@ -I/usr/include -I$(top_srcdir)/bindings -I$(top_srcdir)/bindings/python -I@LIBPRELUDE_PREFIX@/share/libprelude/swig -I$(top_srcdir)/bindings/c++/include -python -threads -O -builtin -o $@ ../libpreludedbcpp.i
endif

endif
endif

-include $(top_srcdir)/git.mk
