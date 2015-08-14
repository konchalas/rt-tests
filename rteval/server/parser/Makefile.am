#   Makefile.am - autotools configuration file
#
#   Copyright 2009   David Sommerseth <davids@redhat.com>
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

# Generic include files, found by ./configure
AM_CPPFLAGS = $(LIBXML2_INC) $(LIBXSLT_INC) $(LIBPQ_INC)

# What is required to build rteval_parserd
bin_PROGRAMS = rteval-parserd
rteval_parserd_SOURCES = argparser.c argparser.h 			 \
	configparser.c configparser.h 					 \
	eurephia_nullsafe.c eurephia_nullsafe.h eurephia_values_struct.h \
	eurephia_values.c eurephia_values.h 				 \
	eurephia_xml.c eurephia_xml.h 					 \
	log.c log.h  							 \
	parsethread.c parsethread.h threadinfo.h			 \
	pgsql.c pgsql.h 						 \
	sha1.c sha1.h							 \
	xmlparser.c xmlparser.h	             				 \
	rteval-parserd.c statuses.h

# Don't build, only install
xsltdir=$(datadir)/rteval
dist_xslt_DATA = xmlparser.xsl

# Copy init script and config file example to the docs dir
initscriptdir=$(docdir)/initscripts
dist_initscript_DATA = rteval-parserd.init rteval-parserd.sysconfig
