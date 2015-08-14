/*
 * Copyright (C) 2009 Red Hat Inc.
 *
 * David Sommerseth <davids@redhat.com>
 *
 *
 * This application is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; version 2.
 *
 * This application is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

/**
 * @file   xmlparser.h
 * @author David Sommerseth <davids@redhat.com>
 * @date   Wed Oct 7 17:27:39 2009
 *
 * @brief Parses summary.xml reports from rteval into a standardised XML format
 *        which is useful when putting data into a database.
 *
 */


#ifndef _XMLPARSER_H
#define _XMLPARSER_H

/**
 *  Parameters needed by the the xmlparser.xsl XSLT template.
 */
typedef struct {
        const char *table;            /**< Which table to parse data for.  Required*/
        unsigned int submid;          /**< Submission ID, needed by the 'rtevalruns' table */
        unsigned int syskey;          /**< System key (referencing systems.syskey) */
        const char *report_filename;  /**< Filename to the saved report (after being parsed) */
        unsigned int rterid;          /**< References rtevalruns.rterid */
} parseParams;


/**
 *  Database specific helper functions
 */
typedef struct {
        char *(*dbh_FormatArray)(LogContext *log, xmlNode *sql_n); /** Formats data as arrays */
} dbhelper_func;

void init_xmlparser(dbhelper_func const * dbhelpers);
char * sqldataValueHash(LogContext *log, xmlNode *sql_n);
xmlDoc *parseToSQLdata(LogContext *log, xsltStylesheet *xslt, xmlDoc *indata_d, parseParams *params);
char *sqldataExtractContent(LogContext *log, xmlNode *sql_n);
int sqldataGetFid(LogContext *log, xmlNode *sqld, const char *fname);
char *sqldataGetValue(LogContext *log, xmlDoc *sqld, const char *fname, int recid);
xmlDoc *sqldataGetHostInfo(LogContext *log, xsltStylesheet *xslt, xmlDoc *summaryxml,
			   int syskey, char **hostname, char **ipaddr);
int sqldataGetRequiredSchemaVer(LogContext *log, xmlNode *sqldata_root);

#endif
