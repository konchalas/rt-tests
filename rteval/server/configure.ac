#   configure.ac - autotools configuration file
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
#   To create the ./configure script you need to run 'autoreconf --install'
#

AC_INIT([rteval-xmlrpc], [1.5], [davids@redhat.com])
SQLSCHEMAVER=1.4
AC_SUBST(SQLSCHEMAVER)

AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CC

AC_ARG_WITH([xmlrpc-webroot],
	[AS_HELP_STRING([--with-xmlrpc-webroot],
			[Location where to install the XML-RPC mod_python files])],
	[XMLRPCROOT="$withval"]
)
AC_SUBST(XMLRPCROOT)
AM_CONDITIONAL([ENAB_XMLRPC], [test ! -z $XMLRPCROOT])

AC_ARG_ENABLE([mod-python],
        [AS_HELP_STRING([--with-mod-python],
                        [Enable the older mod_python support instead of mod_wsgi])],
        [MODPYTHON="$enableval"]
)
AC_SUBST(MODPYTHON)
AM_CONDITIONAL([ENAB_MODPYTHON], [test ! -z $MODPYTHON])

# Simple macro to abort on missing functions in libraries
AC_DEFUN([AX_msgMISSINGFUNC], AC_MSG_ERROR([Could not find function in library.  Aborting]))

# Save original CPPFLAGS
save_CPPFLAGS="$CPPFLAGS"

# Check for libxml2
AC_CHECK_PROGS([XML2CFG], [xml2-config], [:])
if test "$XML2CFG" = :; then
   AC_MSG_ERROR([This package needs xml2-config from libxml2])
else
   AC_SUBST([LIBXML2_INC], [$(xml2-config --cflags)])
   CPPFLAGS="$CPPFLAGS $LIBXML2_INC"
fi
AC_CHECK_HEADERS([libxml/tree.h])
AC_CHECK_HEADERS([libxml/xmlsave.h])
AC_CHECK_HEADERS([libxml/xmlstring.h])
AC_CHECK_HEADERS([libxml/parser.h])
AC_CHECK_LIB([xml2],[xmlInitParser], [], AX_msgMISSINGFUNC)
AC_CHECK_LIB([xml2],[xmlCleanupParser], [DUMMY=], AX_msgMISSINGFUNC)
AC_CHECK_LIB([xml2],[xmlStrcmp], [DUMMY=], AX_msgMISSINGFUNC)
AC_CHECK_LIB([xml2],[xmlFreeDoc], [DUMMY=], AX_msgMISSINGFUNC)
AC_CHECK_LIB([xml2],[xmlSaveToBuffer], [DUMMY=], AX_msgMISSINGFUNC)
AC_CHECK_LIB([xml2],[xmlSaveTree], [DUMMY=], AX_msgMISSINGFUNC)
AC_CHECK_LIB([xml2],[xmlSaveClose], [DUMMY=], AX_msgMISSINGFUNC)
AC_CHECK_LIB([xml2],[xmlBufferFree], [DUMMY=], AX_msgMISSINGFUNC)
AC_CHECK_LIB([xml2],[xmlParseFile], [DUMMY=], AX_msgMISSINGFUNC)

# Check for libxslt
AC_CHECK_PROGS([XSLTCFG], [xslt-config], [:])
if test "$XSLTCFG" = :; then
   AC_MSG_ERROR([This package needs xslt-config from libxslt])
else
   AC_SUBST([LIBXSLT_INC], [$(xslt-config --cflags)])
   CPPFLAGS="$CPPFLAGS $LIBXSLT_INC"
fi
AC_CHECK_HEADERS([libxslt/xsltInternals.h])
AC_CHECK_HEADERS([libxslt/transform.h])
AC_CHECK_HEADERS([libxslt/xsltutils.h])
AC_CHECK_LIB([xslt], [xsltInit], [], AX_msgMISSINGFUNC())
AC_CHECK_LIB([xslt], [xsltCleanupGlobals], [DUMMY=], AX_msgMISSINGFUNC())
AC_CHECK_LIB([xslt], [xsltParseStylesheetFile], [DUMMY=], AX_msgMISSINGFUNC())
AC_CHECK_LIB([xslt], [xsltApplyStylesheet], [DUMMY=], AX_msgMISSINGFUNC())
AC_CHECK_LIB([xslt], [xsltFreeStylesheet], [DUMMY=], AX_msgMISSINGFUNC())

# Check for libpq
AC_CHECK_PROGS([PGSQLCFG], [pg_config], [:])
if test "$PGSQLCFG" = :; then
   AC_MSG_ERROR([This package needs pg_config from PostgreSQL])
else
   AC_SUBST([LIBPQ_INC], [-I$(pg_config --includedir)])
   CPPFLAGS="$CPPFLAGS $LIBPQ_INC"
fi
AC_CHECK_HEADERS([libpq-fe.h])
AC_CHECK_LIB([pq], [PQsetdbLogin], [], AX_msgMISSINGFUNC())
AC_CHECK_LIB([pq], [PQstatus], [DUMMY=], AX_msgMISSINGFUNC())
AC_CHECK_LIB([pq], [PQexec], [DUMMY=], AX_msgMISSINGFUNC())
AC_CHECK_LIB([pq], [PQreset], [DUMMY=], AX_msgMISSINGFUNC())
AC_CHECK_LIB([pq], [PQfinish], [DUMMY=], AX_msgMISSINGFUNC())
AC_CHECK_LIB([pq], [PQprepare], [DUMMY=], AX_msgMISSINGFUNC())
AC_CHECK_LIB([pq], [PQexecPrepared], [DUMMY=], AX_msgMISSINGFUNC())
AC_CHECK_LIB([pq], [PQresultStatus], [DUMMY=], AX_msgMISSINGFUNC())
AC_CHECK_LIB([pq], [PQclear], [DUMMY=], AX_msgMISSINGFUNC())
AC_CHECK_LIB([pq], [PQconsumeInput], [DUMMY=], AX_msgMISSINGFUNC())
AC_CHECK_LIB([pq], [PQnotifies], [DUMMY=], AX_msgMISSINGFUNC())
AC_CHECK_LIB([pq], [PQntuples], [DUMMY=], AX_msgMISSINGFUNC())

# Restore the original CPPFLAGS
CPPFLAGS="$save_CPPFLAGS"

# Check for POSIX features
AC_CHECK_HEADERS([mq.h])
AC_CHECK_LIB([rt], [mq_open], [], AX_msgMISSINGFUNC())
AC_CHECK_LIB([rt], [mq_close], [DUMMY=], AX_msgMISSINGFUNC())
AC_CHECK_LIB([rt], [mq_unlink], [DUMMY=], AX_msgMISSINGFUNC())
AC_CHECK_LIB([rt], [mq_send], [DUMMY=], AX_msgMISSINGFUNC())
AC_CHECK_LIB([rt], [mq_receive], [DUMMY=], AX_msgMISSINGFUNC())

AC_CHECK_HEADERS([pthread.h])
AC_CHECK_LIB([pthread], [pthread_attr_init], [], AX_msgMISSINGFUNC())
AC_CHECK_LIB([pthread], [pthread_attr_setdetachstate], [DUMMY=], AX_msgMISSINGFUNC())
AC_CHECK_LIB([pthread], [pthread_attr_destroy], [DUMMY=], AX_msgMISSINGFUNC())
AC_CHECK_LIB([pthread], [pthread_create], [DUMMY=], AX_msgMISSINGFUNC())
AC_CHECK_LIB([pthread], [pthread_join], [DUMMY=], AX_msgMISSINGFUNC())
AC_CHECK_LIB([pthread], [pthread_mutex_lock], [DUMMY=], AX_msgMISSINGFUNC())
AC_CHECK_LIB([pthread], [pthread_mutex_unlock], [DUMMY=], AX_msgMISSINGFUNC())

# Back to needed autotools stuff
AC_CONFIG_SRCDIR([parser/rteval-parserd.c])
AC_CONFIG_HEADERS([parser/config.h])
AC_CONFIG_FILES([Makefile parser/Makefile])

AC_OUTPUT
