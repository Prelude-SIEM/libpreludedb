%{!?python_sitelib: %global python_sitelib %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib()")}
%{!?python_sitearch: %global python_sitearch %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib(1)")}

Name:  libpreludedb
Epoch:  1
Version: 1.0.1
Release: 1%{?dist}
Summary: Provide the framework for easy access to the Prelude database
Group:  System Environment/Libraries
License: GPLv2+
URL:  http://prelude-ids.com/
Source0: http://prelude-ids.com/download/releases/%{name}/%{name}-%{version}.tar.gz 
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: libprelude-devel python-devel perl-devel
BuildRequires: sqlite-devel mysql-devel, postgresql-devel

%description
The PreludeDB Library provides an abstraction layer upon the type and the
format of the database used to store IDMEF alerts. It allows developers
to use the Prelude IDMEF database easily and efficiently without
worrying about SQL, and to access the database independently of the
type/format of the database.

%package devel
Summary: Libraries and headers for PreludeDB
Group: System Environment/Libraries
Requires: libpreludedb = %{epoch}:%{version}-%{release}, libprelude-devel, automake

%description devel
The PreludeDB Library provides an abstraction layer upon the type
and the format of the database used to store IDMEF alerts. It
allows developers to use the Prelude IDMEF database easily and
efficiently without worrying about SQL, and to access the
database independently of the type/format of the database.

%package mysql
Summary: Plugin to use prelude with a mysql database
Group: System Environment/Libraries
Requires: libpreludedb = %{epoch}:%{version}-%{release}, mysql, mysql-server

%description mysql
This plugin allows prelude to store alerts into a mysql database.

%package pgsql
Summary: Plugin to use prelude with a pgsql database
Group: System Environment/Libraries
Requires: libpreludedb = %{epoch}:%{version}-%{release}, postgresql-server 

%description pgsql
This plugin allows prelude to store alerts into a pgsql database.

%package sqlite
Summary: Plugin to use prelude with a sqlite database
Group: System Environment/Libraries
Requires: libpreludedb = %{epoch}:%{version}-%{release}, sqlite

%description sqlite
This plugin allows prelude to store alerts into a sqlite database.

%package python
Summary: Python bindings for libpreludedb
Group: System Environment/Libraries
Requires: libpreludedb = %{epoch}:%{version}-%{release}

%description python
Python bindings for libpreludedb.

%package perl
Summary: Perl bindings for libpreludedb
Group: System Environment/Libraries
Requires: libpreludedb = %{epoch}:%{version}-%{release}
Requires: perl(:MODULE_COMPAT_%(eval "`%{__perl} -V:version`"; echo $version))

%description perl
Perl bindings for libpreludedb.


%prep
%setup -q


%build
%configure --with-html-dir=%{_defaultdocdir}/%{name}-%{version}/html \
  --with-perl-installdirs=vendor \
  --disable-static \
  --enable-gtk-doc

# removing rpath
sed -i.rpath -e 's|LD_RUN_PATH=""||' bindings/Makefile
sed -i.rpath -e 's|^sys_lib_dlsearch_path_spec="/lib /usr/lib|sys_lib_dlsearch_path_spec="/%{_lib} %{_libdir}|' libtool

make %{?_smp_mflags}


%install
rm -rf %{buildroot}
mkdir -p %{buildroot}%{_defaultdocdir}/%{name}-%{version}
make install DESTDIR=%{buildroot} INSTALL="%{__install} -c -p"
cp -p ChangeLog README NEWS COPYING LICENSE.README HACKING.README \
 %{buildroot}%{_defaultdocdir}/%{name}-%{version}
rm -f %{buildroot}/%{_libdir}/%{name}.la
rm -f %{buildroot}/%{_libdir}/%{name}/plugins/formats/classic.la
rm -f %{buildroot}/%{_libdir}/%{name}/plugins/sql/mysql.la
rm -f %{buildroot}/%{_libdir}/%{name}/plugins/sql/pgsql.la
rm -f %{buildroot}/%{_libdir}/%{name}/plugins/sql/sqlite3.la
chmod 755 %{buildroot}%{python_sitearch}/_preludedb.so
find %{buildroot} -type f \( -name .packlist -o -name perllocal.pod \) -exec rm -f {} ';'
find %{buildroot} -type f -name '*.bs' -a -size 0 -exec rm -f {} ';'

%clean
rm -rf %{buildroot}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_bindir}/preludedb-admin
%dir %{_libdir}/%{name}/
%{_libdir}/%{name}*.so.*
%dir %{_libdir}/%{name}/plugins/
%dir %{_libdir}/%{name}/plugins/formats/
%{_libdir}/%{name}/plugins/formats/*
%dir %{_defaultdocdir}/%{name}-%{version}/
%doc %{_defaultdocdir}/%{name}-%{version}/[A-Z]*
%dir %{_libdir}/%{name}/plugins/sql/
%dir %{_datadir}/%{name}
%dir %{_datadir}/%{name}/classic/
%{_mandir}/man1/preludedb-admin.1.gz

%files devel
%defattr(-,root,root)
%{_bindir}/%{name}-config
%{_libdir}/%{name}*.so
%dir %{_includedir}/%{name}/
%{_includedir}/%{name}/*
%{_datadir}/aclocal/libpreludedb.m4
%doc %{_defaultdocdir}/%{name}-%{version}/html/

%files python
%defattr(-,root,root)
%{python_sitearch}/*

%files perl
%defattr(0755,root,root)
%{perl_vendorarch}/auto/PreludeDB/
%attr(0644,root,root) %{perl_vendorarch}/PreludeDB.pm

%files mysql
%defattr(0755,root,root)
%{_libdir}/%{name}/plugins/sql/mysql.so
%attr(0644,root,root) %{_datadir}/%{name}/classic/mysql*.sql
%attr(0755,root,root) %{_datadir}/%{name}/classic/*.sh

%files sqlite
%defattr(-,root,root)
%{_libdir}/%{name}/plugins/sql/sqlite3.so
%{_datadir}/%{name}/classic/sqlite*

%files pgsql
%defattr(-,root,root)
%{_libdir}/%{name}/plugins/sql/pgsql.so
%{_datadir}/%{name}/classic/pgsql*


%changelog
* Wed Jun 15 2011 Vincent Quéméner <vincent.quemener@c-s.fr> - 1.0.0-8
- Rebuilt for RHEL6

* Wed Mar 23 2011 Dan Horák <dan@danny.cz> - 1:1.0.0-7
- rebuilt for mysql 5.5.10 (soname bump in libmysqlclient)

* Wed Feb 09 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1:1.0.0-6
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Wed Jul 21 2010 David Malcolm <dmalcolm@redhat.com> - 1:1.0.0-5
- Rebuilt for https://fedoraproject.org/wiki/Features/Python_2.7/MassRebuild

* Tue Jun 01 2010 Marcela Maslanova <mmaslano@redhat.com> - 1:1.0.0-4
- Mass rebuild with perl-5.12.0

* Sun May 02 2010 Steve Grubb <sgrubb@redhat.com> 1.0.0-3
- Fix requires

* Fri Apr 30 2010 Steve Grubb <sgrubb@redhat.com> 1.0.0-2
- new upstream release

* Sat Jan 30 2010 Steve Grubb <sgrubb@redhat.com> 1.0.0rc1-1
- new upstream bugfix release

* Thu Dec 17 2009 Steve Grubb <sgrubb@redhat.com> 0.9.15.3-1
- new upstream bugfix release

* Mon Dec  7 2009 Stepan Kasal <skasal@redhat.com> - 0.9.15.1-6
- rebuild against perl 5.10.1

* Tue Aug 25 2009 Steve Grubb <sgrubb@redhat.com> - 0.9.15.1-5
- rebuild for new openssl

* Wed Feb 25 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.9.15.1-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_11_Mass_Rebuild

* Sat Jan 24 2009 Caolán McNamara <caolanm@redhat.com> - 0.9.15.1-3
- rebuild for dependencies

* Sat Nov 29 2008 Ignacio Vazquez-Abrams <ivazqueznet+rpm@gmail.com> - 0.9.15.1-2
- Rebuild for Python 2.6

* Sun Sep 14 2008 Steve Grubb <sgrubb@redhat.com> 0.9.15.1-1
- new upstream bugfix release

* Wed Aug 27 2008 Steve Grubb <sgrubb@redhat.com> 0.9.15-1
- new upstream release

* Fri Jul 04 2008 Steve Grubb <sgrubb@redhat.com> - 0.9.14.1-4
- Fix perl bindings (#453935)

* Wed Jun 25 2008 Tomas Mraz <tmraz@redhat.com> - 0.9.14.1-3
- rebuild with new gnutls
- fix install of perl bindings

* Wed Feb 20 2008 Fedora Release Engineering <rel-eng@fedoraproject.org> - 0.9.14.1-2
- Autorebuild for GCC 4.3

* Mon Jan 14 2008 Steve Grubb <sgrubb@redhat.com> 0.9.14.1-1
- new upstream version 0.9.14.1

* Sun Dec 09 2007 <alexlan at fedoraproject dot org> - 0.9.11.1-4
- Add missing BR: perl-devel

* Thu Dec 06 2007 Release Engineering <rel-eng at fedoraproject dot org> - 0.9.11.1-3
- Rebuild for deps

* Fri Jan 05 2007 Thorsten Scherf <tscherf@redhat.com> 0.9.11.1-2
- moved to new upstream version 0.9.11.1

* Mon Jan 01 2007 Thorsten Scherf <tscherf@redhat.com> 0.9.11-4
- added x86_64-sqlite3.patch to resolve x86_86 build problems

* Sun Dec 31 2006 Thorsten Scherf <tscherf@redhat.com> 0.9.11-3
- resolved macro problem in changelog
- changed several dirowner
- moved html docs into -devel

* Sat Dec 30 2006 Thorsten Scherf <tscherf@redhat.com> 0.9.11-2
- corrected file list entries
- added new BuildReqs to the devel-package
- changed dirowner
- fixed x86_86 arch build problem with %%python_sitearch

* Fri Dec 29 2006 Thorsten Scherf <tscherf@redhat.com> 0.9.11-1
- resolved rpath issue
- added python_sitearch and python_sitelib
- fixed permissions problem
- moved to new upstream version 0.9.11

* Mon Nov 20 2006 Thorsten Scherf <tscherf@redhat.com> 0.9.10-2
- Some minor fixes in requirements

* Tue Oct 24 2006 Thorsten Scherf <tscherf@redhat.com> 0.9.10-1
- New fedora build based on release 0.9.10
- New fedora build based on release 0.9.10

