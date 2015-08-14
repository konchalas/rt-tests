#   Makefile.am - autotools configuration file
#
#   Copyright 2009-2011   David Sommerseth <davids@redhat.com>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
#
#   For the avoidance of doubt the "preferred form" of this code is one which
#   is in an open unpatent encumbered format. Where cryptographic key signing
#   forms part of the process of creating an executable the information
#   including keys needed to generate an equivalently functional executable
#   are deemed to be part of the source code.
#
SUBDIRS = parser
dist_doc_DATA = parser/README.parser 	\
	sql/delta-1.0_1.1.sql 		\
	sql/delta-1.1_1.2.sql 		\
	sql/delta-1.2_1.3.sql 		\
	sql/delta-1.3_1.4.sql 		\
	sql/rteval-$(SQLSCHEMAVER).sql

apache-rteval.conf:
if ENAB_MODPYTHON
	[ -n $(XMLRPCROOT) ] && $(srcdir)/gen_config.sh apache-rteval.conf $(XMLRPCROOT)/API1
else
	[ -n $(XMLRPCROOT) ] && $(srcdir)/gen_config.sh apache-rteval-wsgi.conf $(XMLRPCROOT)/API1
endif

clean-local:
	-rm -f apache-rteval.conf *~

dist-hook:
	cp $(srcdir)/gen_config.sh $(srcdir)/apache-rteval.conf.tpl $(srcdir)/apache-rteval-wsgi.conf.tpl $(distdir)/
	-rm -f $(distdir)/apache-rteval.conf

if ENAB_XMLRPC
    xmlrpcdir = $(XMLRPCROOT)/API1
    BUILT_SOURCES = apache-rteval.conf
    dist_doc_DATA += README.xmlrpc apache-rteval.conf
    dist_xmlrpc_DATA = xmlrpc_API1.py rtevaldb.py database.py
if ENAB_MODPYTHON
    dist_xmlrpc_DATA += rteval_xmlrpc.py
else
    dist_xmlrpc_DATA += rteval_xmlrpc.wsgi
endif
endif
