libtoolize -c --force || (echo aborting ; exit)
aclocal || (echo aborting ; exit)
autoheader-2.53 || (echo aborting ; exit)
automake --gnu -a -c || (echo aborting ; exit)
autoconf-2.53 || (echo aborting ; exit)
#./configure
