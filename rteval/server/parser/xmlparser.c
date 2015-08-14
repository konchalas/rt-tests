/*
 * Copyright (C) 2009 Red Hat Inc.
 *
 * David Sommerseth <davids@redhat.com>
 *
 * Parses summary.xml reports from rteval into a standardised XML format
 * which is useful when putting data into a database.
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
 * @file   xmlparser.c
 * @author David Sommerseth <davids@redhat.com>
 * @date   Wed Oct 21 10:58:53 2009
 *
 * @brief Parses summary.xml reports from rteval into a standardised XML format
 *        which is useful when putting data into a database.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <libxml/tree.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

#include <eurephia_nullsafe.h>
#include <eurephia_xml.h>
#include <xmlparser.h>
#include <sha1.h>
#include <log.h>

static dbhelper_func const * xmlparser_dbhelpers = NULL;

/**
 * Simple strdup() function which encapsulates the string in single quotes,
 * which is needed for XSLT parameter values
 *
 * @param str The string to be strdup'ed and encapsulated
 *
 * @return Returns a pointer to the new buffer.
 */
static char *encapsString(const char *str) {
        char *ret = NULL;

        if( str == NULL ) {
                return NULL;
        }

        ret = (char *) calloc(1, strlen(str)+4);
        assert( ret != NULL );

        snprintf(ret, strlen(str)+3, "'%s'", str);
        return ret;
}


/**
 * Converts an integer to string an encapsulates the value in single quotes,
 * which is needed for XSLT parameter values.
 *
 * @param val Integer value to encapsulate
 *
 * @return Returns a pointer to a new buffer with the encapsulated integer value.  This
 *         buffer must be free'd after usage.
 */
static char *encapsInt(const unsigned int val) {
        char *buf = NULL;

        buf = (char *) calloc(1, 130);
        snprintf(buf, 128, "'%i'", val);
        return buf;
}


/**
 * Simple function to determine if the given string is a number or not
 *
 * @param str Pointer to the tring to be checked
 *
 * @returns Returns 0 if not a number and a non-null value if it is a number
 */
int isNumber(const char * str)
{
    char *ptr = NULL;

    if (str == NULL || *str == '\0' || isspace(*str))
      return 0;

    strtod (str, &ptr);
    return *ptr == '\0';
}

/**
 * Initialise the XML parser, setting some global variables
 */
void init_xmlparser(dbhelper_func const * dbhelpers)
{
	xmlparser_dbhelpers = dbhelpers;
}


/**
 * Parses any XML input document into a sqldata XML format which can be used by pgsql_INSERT().
 * The transformation must be defined in the input XSLT template.
 *
 * @param log       Log context
 * @param xslt      XSLT template defining the data transformation
 * @param indata_d  Input XML data to transform to a sqldata XML document
 * @param params    Parameters to be sent to the XSLT parser
 *
 * @return Returns a well formed sqldata XML document on success, otherwise NULL is returned.
 */
xmlDoc *parseToSQLdata(LogContext *log, xsltStylesheet *xslt, xmlDoc *indata_d, parseParams *params) {
        xmlDoc *result_d = NULL;
        char *xsltparams[10];
        unsigned int idx = 0, idx_table = 0, idx_submid = 0,
		idx_syskey = 0, idx_rterid = 0, idx_repfname = 0;

        if( xmlparser_dbhelpers == NULL ) {
                writelog(log, LOG_ERR, "Programming error: xmlparser is not initialised");
                return NULL;
        }

        if( params->table == NULL ) {
                writelog(log, LOG_ERR, "Table is not defined");
                return NULL;
        }

        // Prepare XSLT parameters
        xsltparams[idx++] = "table\0";
        xsltparams[idx] = (char *) encapsString(params->table);
        idx_table = idx++;

        if( params->submid > 0) {
                xsltparams[idx++] = "submid\0";
                xsltparams[idx] = (char *) encapsInt(params->submid);
                idx_submid = idx++;
        }

        if( params->syskey > 0) {
                xsltparams[idx++] = "syskey\0";
                xsltparams[idx] = (char *) encapsInt(params->syskey);
                idx_syskey = idx++;
        }

        if( params->rterid > 0 ) {
                xsltparams[idx++] = "rterid";
                xsltparams[idx] = (char *) encapsInt(params->rterid);
                idx_rterid = idx++;
        }

        if( params->report_filename ) {
                xsltparams[idx++] = "report_filename";
                xsltparams[idx] = (char *) encapsString(params->report_filename);
                idx_repfname = idx++;
        }
        xsltparams[idx] = NULL;

        // Apply the XSLT template to the input XML data
        result_d = xsltApplyStylesheet(xslt, indata_d, (const char **)xsltparams);
        if( result_d == NULL ) {
                writelog(log, LOG_CRIT, "Failed applying XSLT template to input XML");
        }

        // Free memory we allocated via encapsString()/encapsInt()
        free(xsltparams[idx_table]);
        if( params->submid ) {
                free(xsltparams[idx_submid]);
        }
        if( params->syskey ) {
                free(xsltparams[idx_syskey]);
        }
        if( params->rterid ) {
                free(xsltparams[idx_rterid]);
        }
        if( params->report_filename ) {
                free(xsltparams[idx_repfname]);
        }

        return result_d;
}


/**
 * Internal xmlparser function.   Extracts the value from a '//sqldata/records/record/value'
 * node and hashes the value if the 'hash' attribute is set.  Otherwise the value is extracted
 * from the node directly.  This function is only used by sqldataExtractContent().
 *
 * @param sql_n sqldata values node containing the value to extract.
 *
 * @return Returns a pointer to a new buffer containing the value on success, otherwise NULL.
 *         This memory buffer must be free'd after usage.
 */
char * sqldataValueHash(LogContext *log, xmlNode *sql_n) {
	const char *hash = NULL, *isnull = NULL;
	SHA1Context shactx;
	uint8_t shahash[SHA1_HASH_SIZE];
	char *ret = NULL, *ptr = NULL;
	int i;

	if( !(sql_n && (xmlStrcmp(sql_n->name, (xmlChar *) "value") == 0)
              && (xmlStrcmp(sql_n->parent->name, (xmlChar *) "record") == 0)
              || (xmlStrcmp(sql_n->parent->name, (xmlChar *) "value") == 0)) ) {
                return NULL;
	}

	isnull = xmlGetAttrValue(sql_n->properties, "isnull");
	if( isnull && (strcmp(isnull, "1") == 0) ) {
		return NULL;
	}

	hash = xmlGetAttrValue(sql_n->properties, "hash");
	if( !hash ) {
		// If no hash attribute is found, just use the raw data
		ret = strdup_nullsafe(xmlExtractContent(sql_n));
	} else if( strcasecmp(hash, "sha1") == 0 ) {
		const char *indata = xmlExtractContent(sql_n);
		// SHA1 hashing requested
		SHA1Init(&shactx);
		SHA1Update(&shactx, indata, strlen_nullsafe(indata));
		SHA1Final(&shactx, shahash);

		// "Convert" to a readable format
		ret = malloc_nullsafe(log, (SHA1_HASH_SIZE * 2) + 3);
		ptr = ret;
		for( i = 0; i < SHA1_HASH_SIZE; i++ ) {
			sprintf(ptr, "%02x", shahash[i]);
			ptr += 2;
		}
	} else {
		ret = strdup("<Unsupported hashing algorithm>");
	}

	return ret;
}


/**
 * Extract the content of a //sqldata/records/record/value[@type='array']/value node set
 * and format it in suitable array format for the database backend.
 *
 * @param log    Log context
 * @param sql_n sqldata values node containing the value to extract and format as an array.
 *
 * @return Returns a pointer to a new memory buffer containing the value as a string.
 *         On errors, NULL is returned.  This memory buffer must be free'd after usage.
 */
static char * sqldataValueArray(LogContext *log, xmlNode *sql_n)
{
	if( xmlparser_dbhelpers == NULL ) {
		writelog(log, LOG_ERR, "Programming error: xmlparser is not initialised");
		return NULL;
	}

	return xmlparser_dbhelpers->dbh_FormatArray(log, sql_n);
}


/**
 * Extract the content of a '//sqldata/records/record/value' node.  It will consider
 * both the 'hash' and 'type' attributes of the 'value' tag.
 *
 * @param log    Log context
 * @param sql_n  Pointer to a value node of a sqldata XML document.
 *
 * @return Returns a pointer to a new memory buffer containing the value as a string.
 *         On errors, NULL is returned.  This memory buffer must be free'd after usage.
 */
char *sqldataExtractContent(LogContext *log, xmlNode *sql_n) {
	const char *valtype = xmlGetAttrValue(sql_n->properties, "type");

        if( xmlparser_dbhelpers == NULL ) {
                writelog(log, LOG_ERR, "Programming error: xmlparser is not initialised");
                return NULL;
        }

	if( !sql_n || (xmlStrcmp(sql_n->name, (xmlChar *) "value") != 0)
	    || (xmlStrcmp(sql_n->parent->name, (xmlChar *) "record") != 0) ) {
		    return NULL;
	}

	if( valtype && (strcmp(valtype, "xmlblob") == 0) ) {
		xmlNode *chld_n = sql_n->children;

		// Go to next "real" tag, skipping non-element nodes
		while( chld_n && chld_n->type != XML_ELEMENT_NODE ){
			chld_n = chld_n->next;
		}
		return xmlNodeToString(log, chld_n);
        } else if( valtype && (strcmp(valtype, "array") == 0) ) {
                return sqldataValueArray(log, sql_n);
	} else {
		return sqldataValueHash(log, sql_n);
	}
}


/**
 * Return the 'fid' value of a given field in an sqldata XML document.
 *
 * @param log    Log context
 * @param sql_n  Pointer to the root xmlNode element of a sqldata XML document
 * @param fname  String containing the field name to look up
 *
 * @return Returns a value >= 0 on success, containing the 'fid' value of the field.  Otherwise
 *         a value < 0 is returned.  -1 if the field is not found or -2 if there are some problems
 *         with the XML document.
 */
int sqldataGetFid(LogContext *log, xmlNode *sql_n, const char *fname) {
	xmlNode *f_n = NULL;

        if( xmlparser_dbhelpers == NULL ) {
                writelog(log, LOG_ERR, "Programming error: xmlparser is not initialised");
                return -2;
        }

	if( !sql_n || (xmlStrcmp(sql_n->name, (xmlChar *) "sqldata") != 0) ) {
		writelog(log, LOG_ERR,
			 "sqldataGetFid: Input XML document is not a valid sqldata document");
		return -2;
	}

	f_n = xmlFindNode(sql_n, "fields");
	if( !f_n || !f_n->children ) {
		writelog(log, LOG_ERR,
			 "sqldataGetFid: Input XML document does not contain a fields section");
		return -2;
	}

	foreach_xmlnode(f_n->children, f_n) {
		if( (f_n->type != XML_ELEMENT_NODE)
		    || xmlStrcmp(f_n->name, (xmlChar *) "field") != 0 ) {
			// Skip uninteresting nodes
			continue;
		}

		if( strcmp(xmlExtractContent(f_n), fname) == 0 ) {
			char *fid = xmlGetAttrValue(f_n->properties, "fid");
			if( !fid ) {
				writelog(log, LOG_ERR,
					 "sqldataGetFid: Field node is missing 'fid' attribute (field: %s)",
					 fname);
				return -2;
			}
			return atoi_nullsafe(fid);
		}
	}
	return -1;
}


/**
 * Retrieves the value of a particular field in an sqldata XML document.
 *
 * @param log    Log context
 * @param sqld   pointer to an sqldata XML document.
 * @param fname  String containing the field name to extract the value of.
 * @param recid  Integer containing the record ID of the record to extract the value.  This starts
 *               on 0.
 *
 * @return Returns a pointer to a new memory buffer containing the extracted value.  On errors or if
 *         recid is higher than available records, NULL is returned.
 */
char *sqldataGetValue(LogContext *log, xmlDoc *sqld, const char *fname, int recid ) {
	xmlNode *r_n = NULL;
	int fid = -3, rc = 0;

        if( xmlparser_dbhelpers == NULL ) {
                writelog(log, LOG_ERR, "Programming error: xmlparser is not initialised");
                return NULL;
        }

	if( recid < 0 ) {
		writelog(log, LOG_ERR, "sqldataGetValue: Invalid recid");
		return NULL;
	}

	r_n = xmlDocGetRootElement(sqld);
	if( !r_n || (xmlStrcmp(r_n->name, (xmlChar *) "sqldata") != 0) ) {
		writelog(log, LOG_ERR,
			 "sqldataGetValue: Input XML document is not a valid sqldata document");
		return NULL;
	}

	fid = sqldataGetFid(log, r_n, fname);
	if( fid < 0 ) {
		return NULL;
	}

	r_n = xmlFindNode(r_n, "records");
	if( !r_n || !r_n->children ) {
		writelog(log, LOG_ERR,
			 "sqldataGetValue: Input XML document does not contain a records section");
		return NULL;
	}

	foreach_xmlnode(r_n->children, r_n) {
		if( (r_n->type != XML_ELEMENT_NODE)
		    || xmlStrcmp(r_n->name, (xmlChar *) "record") != 0 ) {
			// Skip uninteresting nodes
			continue;
		}
		if( rc == recid ) {
			xmlNode *v_n = NULL;
			// The rigth record is found, find the field we're looking for
			foreach_xmlnode(r_n->children, v_n) {
				char *fid_s = NULL;
				if( (v_n->type != XML_ELEMENT_NODE)
				    || (xmlStrcmp(v_n->name, (xmlChar *) "value") != 0) ) {
					// Skip uninteresting nodes
					continue;
				}
				fid_s = xmlGetAttrValue(v_n->properties, "fid");
				if( fid_s && (fid == atoi_nullsafe(fid_s)) ) {
					return sqldataExtractContent(log, v_n);
				}
			}
		}
		rc++;
	}
	return NULL;
}


/**
 * Helper function to parse an sqldata XML document for the systems_hostname table.  In addition
 * it will also return two strings containing hostname and ipaddress of the host.
 *
 * @param log        Log context
 * @param xslt       Pointer to an xmlparser.xml XSLT template
 * @param summaryxml rteval XML report document
 * @param syskey     Integer containing the syskey value corresponding to this host
 * @param hostname   Return pointer for where the hostname will be saved.
 * @param ipaddr     Return pointer for where the IP address will be saved.
 *
 * @return Returns a sqldata XML document on success.  In this case the hostname and ipaddr will point
 *         at memory buffers containing hostname and ipaddress.  These values must be free'd after usage.
 *         On errors the function will return NULL and hostname and ipaddr will not have been touched
 *         at all.
 */
xmlDoc *sqldataGetHostInfo(LogContext *log, xsltStylesheet *xslt, xmlDoc *summaryxml,
			   int syskey, char **hostname, char **ipaddr)
{
	xmlDoc *hostinfo_d = NULL;
	parseParams prms;

        if( xmlparser_dbhelpers == NULL ) {
                writelog(log, LOG_ERR, "Programming error: xmlparser is not initialised");
                return NULL;
        }

	memset(&prms, 0, sizeof(parseParams));
	prms.table = "systems_hostname";
	prms.syskey = syskey;

	hostinfo_d = parseToSQLdata(log, xslt, summaryxml, &prms);
	if( !hostinfo_d ) {
		writelog(log, LOG_ERR,
			 "sqldatGetHostInfo: Could not parse input XML data");
		xmlFreeDoc(hostinfo_d);
		goto exit;
	}

	// Grab hostname from input XML
	*hostname = sqldataGetValue(log, hostinfo_d, "hostname", 0);
	if( !hostname ) {
		writelog(log, LOG_ERR,
			"sqldatGetHostInfo: Could not retrieve the hostname field from the input XML");
		xmlFreeDoc(hostinfo_d);
		goto exit;
	}

	// Grab ipaddr from input XML
	*ipaddr = sqldataGetValue(log, hostinfo_d, "ipaddr", 0);
	if( !ipaddr ) {
		writelog(log, LOG_ERR,
			"sqldatGetHostInfo: Could not retrieve the IP address field from the input XML");
		free_nullsafe(hostname);
		xmlFreeDoc(hostinfo_d);
		goto exit;
	}
 exit:
	return hostinfo_d;
}

int sqldataGetRequiredSchemaVer(LogContext *log, xmlNode *sqldata_root)
{
	char *schver = NULL, *cp = NULL, *ptr = NULL;
	int majv = 0, minv = 0;

        if( xmlparser_dbhelpers == NULL ) {
                writelog(log, LOG_ERR, "Programming error: xmlparser is not initialised");
                return -1;
        }

	if( !sqldata_root || (xmlStrcmp(sqldata_root->name, (xmlChar *) "sqldata") != 0) ) {
		writelog(log, LOG_ERR, "sqldataGetRequiredSchemaVer: Invalid document node");
		return -1;
	}

	schver = xmlGetAttrValue(sqldata_root->properties, "schemaver");
	if( schver == NULL ) {
		return 100;  // If not defined, presume lowest available version.
	}
	cp = strdup(schver);
	assert( cp != NULL );

	if( (ptr = strpbrk(cp, ".")) != NULL ) {
		*ptr = 0;
		ptr++;
		majv = atoi_nullsafe(cp);
		minv = atoi_nullsafe(ptr);
	} else {
		majv = atoi_nullsafe(cp);
		minv = 0;
	}
	free_nullsafe(cp);

	return (majv * 100) + minv;
}
