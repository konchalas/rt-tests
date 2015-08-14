Name:		rteval-parser
Version:	1.5
%define sqlschemaver 1.4
Release:	1%{?dist}
Summary:	Report parser daemon for  rteval XML-RPC
%define pkgname rteval-xmlrpc-%{version}

Group:		Applications/System
License:	GPLv2
URL:		http://git.kernel.org/?p=linux/kernel/git/clrkwllms/rteval.git
Source0:	%{pkgname}.tar.gz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:	postgresql-devel libxml2-devel libxslt-devel
Requires:	postgresql httpd mod_wsgi
Requires(post): chkconfig
Requires(preun): chkconfig
Requires(preun): /sbin/service


%description
The XML parser daemon (rteval-parserd) will parse the received reports
and save them in a database for further processing.

%package -n rteval-xmlrpc
Summary:	XML-RPC server and parser for rteval
BuildArch:	noarch


%description -n rteval-xmlrpc
The XML-RPC server is using Apache and mod_python to receive reports from
rteval clients submitting test results via an XML-RPC API.


%prep
%setup -q -n %{pkgname}


%build
%configure --with-xmlrpc-webroot=%{_localstatedir}/www/html/rteval --docdir=%{_defaultdocdir}/%{pkgname}
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/httpd/conf.d
cp apache-rteval.conf $RPM_BUILD_ROOT/%{_sysconfdir}/httpd/conf.d/rteval-xmlrpc.conf

# Move the init script and config file from docs, to the proper place on RHEL/Fedora
mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/init.d $RPM_BUILD_ROOT/%{_sysconfdir}/sysconfig
mv $RPM_BUILD_ROOT/%{_defaultdocdir}/%{pkgname}/initscripts/rteval-parserd.init $RPM_BUILD_ROOT/%{_sysconfdir}/init.d/rteval-parserd
mv $RPM_BUILD_ROOT/%{_defaultdocdir}/%{pkgname}/initscripts/rteval-parserd.sysconfig $RPM_BUILD_ROOT/%{_sysconfdir}/sysconfig/rteval-parserd
rmdir $RPM_BUILD_ROOT/%{_defaultdocdir}/%{pkgname}/initscripts


%post
/sbin/chkconfig --add rteval-parserd


%preun
if [ "$1" = 0 ] ; then
   /sbin/service rteval-parserd stop > /dev/null 2>&1
   /sbin/chkconfig --del rteval-parserd
fi
exit 0


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
%doc COPYING parser/README.parser sql/rteval-%{sqlschemaver}.sql sql/delta-*_*.sql
%config(noreplace) %{_sysconfdir}/sysconfig/rteval-parserd
%attr(0755,root,root) %{_sysconfdir}/init.d/rteval-parserd
%{_bindir}/rteval-parserd
%{_datadir}/rteval/xmlparser.xsl


%files -n rteval-xmlrpc
%defattr(-,root,root,-)
%doc COPYING README.xmlrpc
%config(noreplace) %{_sysconfdir}/httpd/conf.d/rteval-xmlrpc.conf
%{_localstatedir}/www/html/rteval/


%changelog
* Fri Oct  7 2011 David Sommerseth <dazo@users.sourceforge.net> - 1.5-1
- Added support for storing data as arrays in PostgreSQL
- Updated SQL schema to store CPU topology/core spread as an array in the database

* Fri Feb  4 2011 David Sommerseth <dazo@users.sourceforge.net> - 1.4-1
- Added support for mod_wsgi
- Updated SQL schema, to add rteval annotations to an explicit database column

* Fri Apr  9 2010 David Sommerseth <davids@redhat.com> - 1.3-1
- Updated XML-RPC server, added Hello method

* Fri Mar 26 2010 David Sommerseth <davids@redhat.com> - 1.2-2
- Improved logging

* Fri Mar 26 2010 David Sommerseth <davids@redhat.com> - 1.2-1
- Cleaned up xmlparser.xsl
- Honour 'isnull' attributes in SQL XML
- Improved IP address handling on system registration when ipaddr == NULL
- Fixed wrong GRANT statement in rteval_info table

* Mon Mar 22 2010 David Sommerseth <davids@redhat.com> - 1.1-2
- rteval-xmlrpc.spec renamed to rteval-parser.spec
- Split XML-RPC noarch related files and the binary part with rteval-parserd
- Reorganised the .spec file - rteval-xmlrpc RPM is now a noarch sub-package
- Consider the renamed rteval_parserd -> rteval-parserd
- Install /etc/init.d/rteval-parserd and /etc/sysconfig/rteval-parserd

* Tue Dec  8 2009 David Sommerseth <davids@redhat.com> - 1.1-1
- Updated to rteval-xmlrpc v1.1
  - Added new database table, rteval_info, containing some information about the
    rteval-xmlrpc installation
  - Made rteval_parserd aware of which SQL schema version it is working against
  - Added 'schemaver' attributes to <sqldata/> tags, defining which SQL schema
    version which is needed
  - Added mean_absolute_deviation and variance fields from rteval XML reports to
    the database

* Thu Dec  3 2009 David Sommerseth <davids@redhat.com> - 1.0-1
- Inital rteval-xmlrpc.spec file

