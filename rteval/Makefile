HERE	:=	$(shell pwd)
PACKAGE :=	rteval
VERSION :=      $(shell awk '/Version:/ { print $$2 }' ${PACKAGE}.spec | head -n 1)
D	:=	10
PYSRC	:=	rteval/rteval.py 	\
		rteval/cputopology.py	\
		rteval/cyclictest.py 	\
		rteval/dmi.py 		\
		rteval/hackbench.py 	\
		rteval/__init__.py 	\
		rteval/kcompile.py 	\
		rteval/load.py 		\
		rteval/rtevalclient.py 	\
		rteval/rtevalConfig.py 	\
		rteval/rtevalMailer.py 	\
		rteval/util.py		\
		rteval/xmlout.py

XSLSRC	:=	rteval/rteval_dmi.xsl 	\
		rteval/rteval_text.xsl  \
		rteval/rteval_histogram_raw.xsl 

CONFSRC	:=	rteval/rteval.conf

# XML-RPC related files
XMLRPCVER := 1.5
XMLRPCDIR := server

DESTDIR	:=
PREFIX  :=      /usr
DATADIR	:=	$(DESTDIR)/$(PREFIX)/share
CONFDIR	:=	$(DESTDIR)/etc
MANDIR	:=	$(DESTDIR)/$(PREFIX)/share/man
PYLIB	:= 	$(DESTDIR)$(shell python -c 'import distutils.sysconfig;  print distutils.sysconfig.get_python_lib()')
LOADDIR	:=	loadsource

KLOAD	:=	$(LOADDIR)/linux-4.1.5.tar.xz
BLOAD	:=	$(LOADDIR)/dbench-4.0.tar.gz
LOADS	:=	$(KLOAD) $(BLOAD)

runit:
	[ -d ./run ] || mkdir run
	python rteval/rteval.py -D -L -v --workdir=./run --loaddir=./loadsource --duration=$(D) -f ./rteval/rteval.conf -i ./rteval

load:
	[ -d ./run ] || mkdir run
	python rteval/rteval.py --onlyload -D -L -v --workdir=./run --loaddir=./loadsource -f ./rteval/rteval.conf -i ./rteval
sysreport:
	python rteval/rteval.py -D -v --workdir=./run --loaddir=./loadsource --duration=$(D) -i ./rteval --sysreport

clean:
	rm -f *~ rteval/*~ rteval/*.py[co] *.tar.bz2 *.tar.gz doc/*~ server/rteval-xmlrpc-*.tar.gz

realclean: clean
	[ -f $(XMLRPCDIR)/Makefile ] && make -C $(XMLRPCDIR) maintainer-clean || echo -n
	rm -rf run tarball rpm

install: install_loads install_rteval

install_rteval: installdirs
	if [ "$(DESTDIR)" = "" ]; then \
		python setup.py install; \
	else \
		python setup.py install --root=$(DESTDIR); \
	fi
	install -m 644 rteval/rteval_text.xsl $(DATADIR)/rteval
	install -m 644 rteval/rteval_dmi.xsl $(DATADIR)/rteval
	install -m 644 rteval/rteval_histogram_raw.xsl $(DATADIR)/rteval
	install -m 644 rteval/rteval.conf $(CONFDIR)
	install -m 644 doc/rteval.8 $(MANDIR)/man8/
	gzip -f $(MANDIR)/man8/rteval.8
	chmod 755 $(PYLIB)/rteval/rteval.py
#	ln -s $(PYLIB)/rteval/rteval.py $(DESTDIR)/usr/bin/rteval;

install_loads:	$(LOADS)
	[ -d $(DATADIR)/rteval/loadsource ] || mkdir -p $(DATADIR)/rteval/loadsource
	for l in $(LOADS); do \
		install -m 644 $$l $(DATADIR)/rteval/loadsource; \
	done

installdirs:
	[ -d $(DATADIR)/rteval ] || mkdir -p $(DATADIR)/rteval
	[ -d $(CONFDIR) ] || mkdir -p $(CONFDIR)
	[ -d $(MANDIR)/man8 ]  || mkdir -p $(MANDIR)/man8
	[ -d $(PYLIB) ]   || mkdir -p $(PYLIB)
	[ -d $(DESTDIR)/usr/bin ] || mkdir -p $(DESTDIR)/usr/bin

uninstall:
	rm -f /usr/bin/rteval
	rm -f $(CONFDIR)/rteval.conf
	rm -f $(MANDIR)/man8/rteval.8.gz
	rm -rf $(PYLIB)/rteval
	rm -rf $(DATADIR)/rteval

tarfile:
	rm -rf tarball && mkdir -p tarball/rteval-$(VERSION)/rteval tarball/rteval-$(VERSION)/server tarball/rteval-$(VERSION)/sql
	cp $(PYSRC) tarball/rteval-$(VERSION)/rteval
	cp $(XSLSRC) tarball/rteval-$(VERSION)/rteval
	cp $(CONFSRC) tarball/rteval-$(VERSION)/rteval
	cp -r doc/ tarball/rteval-$(VERSION)
	cp Makefile setup.py rteval.spec COPYING tarball/rteval-$(VERSION)
	tar -C tarball -cjvf rteval-$(VERSION).tar.bz2 rteval-$(VERSION)

rteval-xmlrpc-$(XMLRPCVER).tar.gz :
	cd $(XMLRPCDIR) ;             \
	autoreconf --install ;           \
	./configure --prefix=$(PREFIX) ; \
	make distcheck
	cp $(XMLRPCDIR)/rteval-xmlrpc-$(XMLRPCVER).tar.gz $(HERE)/

rpm_prep:
	rm -rf rpm
	mkdir -p rpm/{BUILD,RPMS,SRPMS,SOURCES,SPECS}

rpms rpm: rpm_prep rtevalrpm loadrpm xmlrpcrpm

rtevalrpm: tarfile
	cp rteval-$(VERSION).tar.bz2 rpm/SOURCES
	cp rteval.spec rpm/SPECS
	rpmbuild -ba --define "_topdir $(HERE)/rpm" rpm/SPECS/rteval.spec

xmlrpcrpm: rteval-xmlrpc-$(XMLRPCVER).tar.gz
	cp rteval-xmlrpc-$(XMLRPCVER).tar.gz rpm/SOURCES/
	cp server/rteval-parser.spec rpm/SPECS/
	rpmbuild -ba --define "_topdir $(HERE)/rpm" rpm/SPECS/rteval-parser.spec

loadrpm: 
	rm -rf rpm-loads
	mkdir -p rpm-loads/{BUILD,RPMS,SRPMS,SOURCES,SPECS}
	cp rteval-loads.spec rpm-loads/SPECS
	cp $(LOADS) rpm-loads/SOURCES
	rpmbuild -ba --define "_topdir $(HERE)/rpm-loads" rpm-loads/SPECS/rteval-loads.spec

rpmlint: rpms
	@echo "==============="
	@echo "running rpmlint"
	rpmlint -v $(shell find ./rpm -type f -name "*.rpm") 	 \
		$(shell find ./rpm-loads -type f -name "*.rpm")	 \
		$(shell find ./rpm/SPECS -type f -name "rteval*.spec") \
		$(shell find ./rpm-loads/SPECS -type f -name "rteval*.spec" )

help:
	@echo ""
	@echo "rteval Makefile targets:"
	@echo ""
	@echo "        runit:     do a short testrun locally [default]"
	@echo "        rpm:       run rpmbuild for all rpms"
	@echo "        rpmlint:   run rpmlint against all rpms/srpms/specfiles"
	@echo "        tarfile:   create the source tarball"
	@echo "        install:   install rteval locally"
	@echo "        clean:     cleanup generated files"
	@echo "        sysreport: do a short testrun and generate sysreport data"
	@echo ""
