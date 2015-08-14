/*
 * Copyright (C) 2009 Red Hat Inc.
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
 * @file   pgsql.c
 * @author David Sommerseth <davids@redhat.com>
 * @date   Wed Oct 13 17:44:35 2009
 *
 * @brief  Database API for the PostgreSQL database.
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>

#include <libpq-fe.h>

#include <libxml/parser.h>
#include <libxml/xmlsave.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

#include <eurephia_nullsafe.h>
#include <eurephia_xml.h>
#include <eurephia_values.h>
#include <configparser.h>
#include <xmlparser.h>
#include <pgsql.h>
#include <log.h>
#include <statuses.h>

/** forward declaration, to be able to setup dbhelper_func pointers */
static char * pgsql_BuildArray(LogContext *log, xmlNode *sql_n);

/** Helper functions the xmlparser might beed */
static dbhelper_func pgsql_helpers = {
        .dbh_FormatArray = &(pgsql_BuildArray)
};

/**
 * Connect to a database, based on the given configuration
 *
 * @param cfg eurephiaVALUES containing the configuration
 * @param id  Database connection ID.  Used to identify which thread is doing what with the database
 * @param log Log context, where all logging will go
 *
 * @return Returns a database connection context
 */
dbconn *db_connect(eurephiaVALUES *cfg, unsigned int id, LogContext *log) {
	dbconn *ret = NULL;
        PGresult *dbr = NULL;

	ret = (dbconn *) malloc_nullsafe(log, sizeof(dbconn)+2);
	ret->id = id;
	ret->log = log;

	writelog(log, LOG_DEBUG, "[Connection %i] Connecting to database: server=%s:%s, "
		 "database=%s, user=%s", ret->id,
		 eGet_value(cfg, "db_server"), eGet_value(cfg, "db_port"),
		 eGet_value(cfg, "database"), eGet_value(cfg, "db_username"));
	ret->db = PQsetdbLogin(eGet_value(cfg, "db_server"),
			   eGet_value(cfg, "db_port"),
			   NULL, /* pgopt */
			   NULL, /* pgtty */
			   eGet_value(cfg, "database"),
			   eGet_value(cfg, "db_username"),
			   eGet_value(cfg, "db_password"));

	if( !ret->db ) {
		writelog(log, LOG_EMERG,
			 "[Connection %i] Could not connect to the database (unknown reason)", ret->id);
		free_nullsafe(ret);
		return NULL;
	}

	if( PQstatus(ret->db) != CONNECTION_OK ) {
		writelog(log, LOG_EMERG, "[Connection %i] Failed to connect to the database: %s",
			 ret->id, PQerrorMessage(ret->db));
		free_nullsafe(ret);
		return NULL;
	}

	// Retrieve the SQL schema version
	dbr = PQexec(ret->db,
		     "SELECT FLOOR(value::NUMERIC(6,3))*100 " // Convert version string to integer
		     "       + to_char(substring(value, position('.' in value)+1)::INTEGER, '00')::INTEGER"
		     "  FROM rteval_info WHERE key = 'sql_schema_ver'");
	if( !dbr || (PQresultStatus(dbr) != PGRES_TUPLES_OK) || (PQntuples(dbr) != 1) ) {
		// Query failed, assuming SQL schema version 1.00 (100).
		// SQL schema versions before 1.1 (101) do not have the rteval_info table, thus
		// a failure is not completely unexpected.
		ret->sqlschemaver = 100;
	} else {
		ret->sqlschemaver = atoi_nullsafe(PQgetvalue(dbr, 0, 0));
		if( ret->sqlschemaver < 100 ) {
			ret->sqlschemaver = 100;  // The minimal version - version 1.00.
		}
	}
	if( dbr ) {
		PQclear(dbr);
	}
	init_xmlparser(&pgsql_helpers);
	return ret;
}


/**
 * Pings the database connection to check if it is alive
 *
 * @param dbc  Database connection to ping
 *
 * @return Returns 1 if the connection is alive, otherwise 0
 */
int db_ping(dbconn *dbc) {
	PGresult *res = NULL;

	// Send ping
	res = PQexec(dbc->db, "");
	PQclear(res);

	// Check status
	if( PQstatus(dbc->db) != CONNECTION_OK ) {
		PQreset(dbc->db);
		if( PQstatus(dbc->db) != CONNECTION_OK ) {
			writelog(dbc->log, LOG_EMERG,
				 "[Connection %i] Database error - Lost connection: %s",
				 dbc->id, PQerrorMessage(dbc->db));
			return 0;
		} else {
			writelog(dbc->log, LOG_CRIT,
				 "[Conncetion %i] Database connection restored", dbc->id);
		}
	}
	return 1;
}


/**
 * Disconnect from the database
 *
 * @param dbc Pointer to the database handle to be disconnected.
 */
void db_disconnect(dbconn *dbc) {
	if( dbc && dbc->db ) {
		writelog(dbc->log, LOG_DEBUG, "[Connection %i] Disconnecting from database", dbc->id);
		PQfinish(dbc->db);
		dbc->db = NULL;
		dbc->log = NULL;
	}
	free_nullsafe(dbc);
}


/**
 * This function does INSERT SQL queries based on an XML document (sqldata) which contains
 * all information about table, fields and records to be inserted.  For security and performance,
 * this function uses prepared SQL statements.
 *
 * This function is PostgreSQL specific.
 *
 * @param dbc     Database handler to a PostgreSQL
 * @param sqldoc  sqldata XML document containing the data to be inserted.
 *
 * The sqldata XML document must be formated like this:
 * @code
 * <sqldata table="{table name}" [key="{field name}">
 *    <fields>
 *       <field fid="{integer}">{field name}</field>
 *       ...
 *       ...
 *       <field fid="{integer_n}">{field name 'n'}</field>
 *    </fields>
 *    <records>
 *       <record>
 *          <value fid="{integer} [type="{data type}"] [hash="{hash type}">{value for field 'fid'</value>
 *          ...
 *          ...
 *          <value fid="{integer_n}">{value for field 'fid_n'</value>
 *       </record>
 *       ...
 *       ...
 *       ...
 *    </records>
 * </sqldata>
 * @endcode
 * The 'sqldata' root tag must contain a 'table' attribute.  This must contain the a name of a table
 * in the database.  If the 'key' attribute is set, the function will return the that field value for
 * each INSERT query, using INSERT ... RETURNING {field name}.  The sqldata root tag must then have
 * two children, 'fields' and 'records'.
 *
 * The 'fields' tag need to contain 'field' children tags for each field to insert data for.  Each
 * field in the fields tag must be assigned a unique integer.
 *
 * The 'records' tag need to contain 'record' children tags for each record to be inserted.  Each
 * record tag needs to have 'value' tags for each field which is found in the 'fields' section.
 *
 * The 'value' tags must have a 'fid' attribute.  This is the link between the field name in the
 * 'fields' section and the value to be inserted.
 *
 * The 'type' attribute may be used as well, but the only supported data type supported to this
 * attribute is 'xmlblob'.  In this case, the contents of the 'value' tag must be more XML tags.
 * These tags will then be serialised to a string which is inserted into the database.
 *
 * The 'hash' attribute of the 'value' tag can be set to 'sha1'.  This will make do a SHA1 hash
 * calculation of the value and this hash value will be used for the insert.
 *
 * @return Returns an eurephiaVALUES list containing information about each record which was inserted.
 *         If the 'key' attribute is not set in the 'sqldata' tag, the OID value of each record will be
 *         saved.  If the table do not support OIDs, the value will be '0'.  Otherwise the contents of
 *         the defined field name will be returned.  If one of the INSERT queries fails, it will abort
 *         further processing and the function will return NULL.
 */
eurephiaVALUES *pgsql_INSERT(dbconn *dbc, xmlDoc *sqldoc) {
	xmlNode *root_n = NULL, *fields_n = NULL, *recs_n = NULL, *ptr_n = NULL, *val_n = NULL;
	char **field_ar = NULL, *fields = NULL, **value_ar = NULL, *values = NULL, *table = NULL, 
		tmp[20], *sql = NULL, *key = NULL, oid[34];

	unsigned int fieldcnt = 0, *field_idx, i = 0, schemaver = 0;
	PGresult *dbres = NULL;
	eurephiaVALUES *res = NULL;

	assert( (dbc != NULL) && (sqldoc != NULL) );

	root_n = xmlDocGetRootElement(sqldoc);
	if( !root_n || (xmlStrcmp(root_n->name, (xmlChar *) "sqldata") != 0) ) {
		writelog(dbc->log, LOG_ERR,
			 "[Connection %i] Input XML document is not a valid sqldata document", dbc->id);
		return NULL;
	}

	table = xmlGetAttrValue(root_n->properties, "table");
	if( !table ) {
		writelog(dbc->log, LOG_ERR,
			 "[Connection %i] Input XML document is missing table reference", dbc->id);
		return NULL;
	}

	schemaver = sqldataGetRequiredSchemaVer(dbc->log, root_n);
	if( schemaver < 100 ) {
		writelog(dbc->log, LOG_ERR,
			 "[Connection %i] Failed parsing required SQL schema version", dbc->id);
		return NULL;
	}
	if( schemaver > dbc->sqlschemaver ) {
		writelog(dbc->log, LOG_ERR,
			 "[Connection %i] Cannot process data for the '%s' table.  "
			 "The needed SQL schema version is %i, while the database is using version %i",
			 dbc->id, table, schemaver, dbc->sqlschemaver);
		return NULL;
	}

	key = xmlGetAttrValue(root_n->properties, "key");

	fields_n = xmlFindNode(root_n, "fields");
	recs_n = xmlFindNode(root_n, "records");
	if( !fields_n || !recs_n ) {
		writelog(dbc->log, LOG_ERR,
			 "[Connection %i] Input XML document is missing either <fields/> or <records/>",
			 dbc->id);
		return NULL;
	}

	// Count number of fields
	foreach_xmlnode(fields_n->children, ptr_n) {
		if( ptr_n->type == XML_ELEMENT_NODE ) {
			fieldcnt++;
		}
	}

	// Generate lists of all fields and a index mapping table
	field_idx = calloc(fieldcnt+1, sizeof(unsigned int));
	field_ar = calloc(fieldcnt+1, sizeof(char *));
	foreach_xmlnode(fields_n->children, ptr_n) {
		if( ptr_n->type != XML_ELEMENT_NODE ) {
			continue;
		}

		field_idx[i] = atoi_nullsafe(xmlGetAttrValue(ptr_n->properties, "fid"));
		field_ar[i] = xmlExtractContent(ptr_n);
		i++;
	}

	// Generate strings with field names and value place holders
	// for a prepared SQL statement
	fields = malloc_nullsafe(dbc->log, 3);
	values = malloc_nullsafe(dbc->log, 6*(fieldcnt+1));
	strcpy(fields, "(");
	strcpy(values, "(");
	int len = 3;
	for( i = 0; i < fieldcnt; i++ ) {
		// Prepare VALUES section
		snprintf(tmp, 6, "$%i", i+1);
		append_str(values, tmp, (6*fieldcnt));

		// Prepare fields section
		len += strlen_nullsafe(field_ar[i])+2;
		fields = realloc(fields, len);
		strcat(fields, field_ar[i]);

		if( i < (fieldcnt-1) ) {
			strcat(fields, ",");
			strcat(values, ",");
		}
	}
	strcat(fields, ")");
	strcat(values, ")");

	// Build up the SQL query
	sql = malloc_nullsafe(dbc->log,
			      strlen_nullsafe(fields)
			      + strlen_nullsafe(values)
			      + strlen_nullsafe(table)
			      + strlen_nullsafe(key)
			      + 34 /* INSERT INTO  VALUES RETURNING*/
			      );
	sprintf(sql, "INSERT INTO %s %s VALUES %s", table, fields, values);
	if( key ) {
		strcat(sql, " RETURNING ");
		strcat(sql, key);
	}

	// Create a prepared SQL query
#ifdef DEBUG_SQL
	writelog(dbc->log, LOG_DEBUG, "[Connection %i] Preparing SQL statement: %s", dbc->id, sql);
#endif
	dbres = PQprepare(dbc->db, "", sql, fieldcnt, NULL);
	if( PQresultStatus(dbres) != PGRES_COMMAND_OK ) {
		writelog(dbc->log, LOG_ALERT,
			 "[Connection %i] Failed to prepare SQL query: %s",
			 dbc->id, PQresultErrorMessage(dbres));
		PQclear(dbres);
		goto exit;
	}
	PQclear(dbres);

	// Loop through all records and generate SQL statements
	res = eCreate_value_space(dbc->log, 1);
	memset(&oid, 0, 34);
	foreach_xmlnode(recs_n->children, ptr_n) {
		if( ptr_n->type != XML_ELEMENT_NODE ) {
			continue;
		}

		// Loop through all value nodes in each record node and get the values for each field
		value_ar = calloc(fieldcnt, sizeof(char *));
		i = 0;
		foreach_xmlnode(ptr_n->children, val_n) {
			char *fid_s = NULL;
			int fid = -1;

			if( i > fieldcnt ) {
				break;
			}

			if( val_n->type != XML_ELEMENT_NODE ) {
				continue;
			}

			fid_s = xmlGetAttrValue(val_n->properties, "fid");
			fid = atoi_nullsafe(fid_s);
			if( (fid_s == NULL) || (fid < 0) ) {
				continue;
			}
			value_ar[field_idx[i]] = sqldataExtractContent(dbc->log, val_n);
			i++;
		}

		// Insert the record into the database
		dbres = PQexecPrepared(dbc->db, "", fieldcnt,
				       (const char * const *)value_ar, NULL, NULL, 0);
		if( PQresultStatus(dbres) != (key ? PGRES_TUPLES_OK : PGRES_COMMAND_OK) ) {
			writelog(dbc->log, LOG_ALERT, "[Connection %i] Failed to do SQL INSERT query: %s",
				 dbc->id, PQresultErrorMessage(dbres));
			PQclear(dbres);
			eFree_values(res);
			res = NULL;

			// Free up the memory we've used for this record
			for( i = 0; i < fieldcnt; i++ ) {
				free_nullsafe(value_ar[i]);
			}
			free_nullsafe(value_ar);
			goto exit;
		}
		if( key ) {
			// If the /sqldata/@key attribute was set, fetch the returning ID
			eAdd_value(res, key, PQgetvalue(dbres, 0, 0));
		} else {
			snprintf(oid, 33, "%ld%c", (unsigned long int) PQoidValue(dbres), 0);
			eAdd_value(res, "oid", oid);
		}
		PQclear(dbres);

		// Free up the memory we've used for this record
		for( i = 0; i < fieldcnt; i++ ) {
			free_nullsafe(value_ar[i]);
		}
		free_nullsafe(value_ar);
	}

 exit:
	free_nullsafe(sql);
	free_nullsafe(fields);
	free_nullsafe(values);
	free_nullsafe(field_ar);
	free_nullsafe(field_idx);
	return res;
}

/**
 * @copydoc sqldataValueArray()
 */
static char * pgsql_BuildArray(LogContext *log, xmlNode *sql_n) {
	char *ret = NULL, *ptr = NULL;
	xmlNode *node = NULL;
	size_t retlen = 0;

	ret = malloc_nullsafe(log, 2);
	if( ret == NULL ) {
		writelog(log, LOG_ERR,
			 "Failed to allocate memory for a new PostgreSQL array");
		return NULL;
	}
	strncat(ret, "{", 1);

	/* Iterate all ./value/value elements and build up a PostgreSQL specific array */
	foreach_xmlnode(sql_n->children, node) {
		if( (node->type != XML_ELEMENT_NODE)
		    || xmlStrcmp(node->name, (xmlChar *) "value") != 0 ) {
			// Skip uninteresting nodes
			continue;
		}
		ptr = sqldataValueHash(log, node);
		if( ptr ) {
			retlen += strlen(ptr) + 4;
			ret = realloc(ret, retlen);
			if( ret == NULL ) {
				writelog(log, LOG_ERR,
					 "Failed to allocate memory to expand "
					 "array to include '%s'", ptr);
				free_nullsafe(ret);
				free_nullsafe(ptr);
				return NULL;
			}
			/* Newer PostgreSQL servers expects numbers to be without quotes */
			if( isNumber(ptr) == 0 ) {
				/* Data is a string */
				strncat(ret, "'", 1);
				strncat(ret, ptr, strlen(ptr));
				strncat(ret, "',", 2);
			} else {
				/* Data is a number */
				strncat(ret, ptr, strlen(ptr));
				strncat(ret, ",", 1);
			}
			free_nullsafe(ptr);
		}
	}
	/* Replace the last comma with a close-array marker */
	ret[strlen(ret)-1] = '}';
	ret[strlen(ret)] = 0;
	return ret;
}


/**
 * Start an SQL transaction (SQL BEGIN)
 *
 * @param dbc Database handler where to perform the SQL queries
 *
 * @return Returns 1 on success, otherwise -1 is returned
 */
int db_begin(dbconn *dbc) {
	PGresult *dbres = NULL;

	dbres = PQexec(dbc->db, "BEGIN");
	if( PQresultStatus(dbres) != PGRES_COMMAND_OK ) {
		writelog(dbc->log, LOG_ALERT,
			 "[Connection %i] Failed to do prepare a transaction (BEGIN): %s",
			 dbc->id, PQresultErrorMessage(dbres));
		PQclear(dbres);
		return -1;
	}
	PQclear(dbres);
	return 1;
}


/**
 * Commits an SQL transaction (SQL COMMIT)
 *
 * @param dbc Database handler where to perform the SQL queries
 *
 * @return Returns 1 on success, otherwise -1 is returned
 */
int db_commit(dbconn *dbc) {
	PGresult *dbres = NULL;

	dbres = PQexec(dbc->db, "COMMIT");
	if( PQresultStatus(dbres) != PGRES_COMMAND_OK ) {
		writelog(dbc->log, LOG_ALERT,
			 "[Connection %i] Failed to do commit a database transaction (COMMIT): %s",
			 dbc->id, PQresultErrorMessage(dbres));
		PQclear(dbres);
		return -1;
	}
	PQclear(dbres);
	return 1;
}


/**
 * Aborts an SQL transaction (SQL ROLLBACK/ABORT)
 *
 * @param dbc Database handler where to perform the SQL queries
 *
 * @return Returns 1 on success, otherwise -1 is returned
 */
int db_rollback(dbconn *dbc) {
	PGresult *dbres = NULL;

	dbres = PQexec(dbc->db, "ROLLBACK");
	if( PQresultStatus(dbres) != PGRES_COMMAND_OK ) {
		writelog(dbc->log, LOG_CRIT,
			 "[Connection %i] Failed to do abort/rollback a transaction (ROLLBACK): %s",
			 dbc->id, PQresultErrorMessage(dbres));
		PQclear(dbres);
		return -1;
	}
	PQclear(dbres);
	return 1;
}


/**
 * This function blocks until a notification is received from the database
 *
 * @param dbc        Database connection
 * @param shutdown   Pointer to the shutdown flag.  Used to avoid reporting false errors.
 * @param listenfor  Name to be used when calling LISTEN
 *
 * @return Returns 1 on successful waiting, otherwise -1
 */
int db_wait_notification(dbconn *dbc, const int *shutdown, const char *listenfor) {
	int sock, ret = 0;
	PGresult *dbres = NULL;
	PGnotify *notify = NULL;
	fd_set input_mask;
	char *sql = NULL;

	sql = malloc_nullsafe(dbc->log, strlen_nullsafe(listenfor) + 12);
	assert( sql != NULL );

	// Initiate listening
	sprintf(sql, "LISTEN %s", listenfor);
	dbres = PQexec(dbc->db, sql);
	if( PQresultStatus(dbres) != PGRES_COMMAND_OK ) {
		writelog(dbc->log, LOG_ALERT, "[Connection %i] SQL %s",
			 dbc->id, PQresultErrorMessage(dbres));
		free_nullsafe(sql);
		PQclear(dbres);
		return -1;
	}
	PQclear(dbres);

	// Start listening and waiting
	while( ret == 0 ) {
		sock = PQsocket(dbc->db);
		if (sock < 0) {
			// shouldn't happen
			ret = -1;
			break;
		}

		// Wait for something to happen on the database socket
		FD_ZERO(&input_mask);
		FD_SET(sock, &input_mask);
		if (select(sock + 1, &input_mask, NULL, NULL, NULL) < 0) {
			// If the shutdown flag is set, select() will fail due to a signal.  Only
			// report errors if we're not shutting down, or else exit normally with
			// successful waiting.
			if( *shutdown == 0 ) {
				writelog(dbc->log, LOG_CRIT, "[Connection %i] select() failed: %s",
					 dbc->id, strerror(errno));
				ret = -1;
				goto exit;
			} else {
				ret = 1;
			}
			break;
		}

		// Process the event
		PQconsumeInput(dbc->db);

		// Check if connection still is valid
		if( PQstatus(dbc->db) != CONNECTION_OK ) {
			PQreset(dbc->db);
			if( PQstatus(dbc->db) != CONNECTION_OK ) {
				writelog(dbc->log, LOG_EMERG,
					 "[Connection %i] Database connection died: %s",
					 dbc->id, PQerrorMessage(dbc->db));
				ret = -1;
				goto exit;
			}
			writelog(dbc->log, LOG_CRIT,
				 "[Connection %i] Database connection restored", dbc->id);
		}

		while ((notify = PQnotifies(dbc->db)) != NULL) {
			// If a notification was received, inform and exit with success.
			writelog(dbc->log, LOG_DEBUG,
				 "[Connection %i] Received notfication from pid %d",
				 dbc->id, notify->be_pid);
			PQfreemem(notify);
			ret = 1;
			break;
		}
	}

	// Stop listening when we exit
	sprintf(sql, "UNLISTEN %s", listenfor);
	dbres = PQexec(dbc->db, sql);
	if( PQresultStatus(dbres) != PGRES_COMMAND_OK ) {
		writelog(dbc->log, LOG_ALERT, "[Connection %i] SQL %s",
			 dbc->id, PQresultErrorMessage(dbres));
		free_nullsafe(sql);
		ret = -1;
	}
	free_nullsafe(sql);
	PQclear(dbres);

 exit:
	return ret;
}


/**
 * Retrive the first available submitted report
 *
 * @param dbc   Database connection
 * @param mtx   pthread_mutex to avoid parallel access to the submission queue table, to avoid
 *              the same job being retrieved multiple times.
 *
 * @return Returns a pointer to a parseJob_t struct, with the parse job info on success, otherwise NULL
 */
parseJob_t *db_get_submissionqueue_job(dbconn *dbc, pthread_mutex_t *mtx) {
	parseJob_t *job = NULL;
	PGresult *res = NULL;
	char sql[4098];

	job = (parseJob_t *) malloc_nullsafe(dbc->log, sizeof(parseJob_t));

	// Get the first available submission
	memset(&sql, 0, 4098);
	snprintf(sql, 4096,
		 "SELECT submid, filename, clientid"
		 "  FROM submissionqueue"
		 " WHERE status = %i"
		 " ORDER BY submid"
		 " LIMIT 1",
		 STAT_NEW);

	pthread_mutex_lock(mtx);
	res = PQexec(dbc->db, sql);
	if( PQresultStatus(res) != PGRES_TUPLES_OK ) {
		pthread_mutex_unlock(mtx);
		writelog(dbc->log, LOG_ALERT,
			 "[Connection %i] Failed to query submission queue (SELECT): %s",
			 dbc->id, PQresultErrorMessage(res));
		PQclear(res);
		free_nullsafe(job);
		return NULL;
	}

	if( PQntuples(res) == 1 ) {
		job->status = jbAVAIL;
		job->submid = atoi_nullsafe(PQgetvalue(res, 0, 0));
		snprintf(job->filename, 4095, "%.4094s", PQgetvalue(res, 0, 1));
		snprintf(job->clientid,  255, "%.254s", PQgetvalue(res, 0, 2));

		// Update the submission queue status
		if( db_update_submissionqueue(dbc, job->submid, STAT_ASSIGNED) < 1 ) {
			pthread_mutex_unlock(mtx);
			writelog(dbc->log, LOG_ALERT, "[Connection %i] Failed to update "
				 "submission queue statis to STAT_ASSIGNED", dbc->id);
			free_nullsafe(job);
			return NULL;
		}
	} else {
		job->status = jbNONE;
	}
	pthread_mutex_unlock(mtx);
	PQclear(res);
	return job;
}


/**
 * Updates the submission queue table with the new status and the appropriate timestamps
 *
 * @param dbc     Database handler to the rteval database
 * @param submid  Submission ID to update
 * @param status  The new status
 *
 * @return Returns 1 on success, 0 on invalid status ID and -1 on database errors.
 */
int db_update_submissionqueue(dbconn *dbc, unsigned int submid, int status) {
	PGresult *res = NULL;
	char sql[4098];

	memset(&sql, 0, 4098);
	switch( status ) {
	case STAT_ASSIGNED:
	case STAT_RTERIDREG:
	case STAT_REPMOVE:
	case STAT_XMLFAIL:
	case STAT_FTOOBIG:
		snprintf(sql, 4096,
			 "UPDATE submissionqueue SET status = %i"
			 " WHERE submid = %i", status, submid);
		break;

	case STAT_INPROG:
		snprintf(sql, 4096,
			 "UPDATE submissionqueue SET status = %i, parsestart = NOW()"
			 " WHERE submid = %i", status, submid);
		break;

	case STAT_SUCCESS:
	case STAT_UNKNFAIL:
	case STAT_SYSREG:
	case STAT_GENDB:
	case STAT_RTEVRUNS:
	case STAT_CYCLIC:
		snprintf(sql, 4096,
			 "UPDATE submissionqueue SET status = %i, parseend = NOW() WHERE submid = %i",
			 status, submid);
		break;

	default:
	case STAT_NEW:
		writelog(dbc->log, LOG_ERR,
			 "[Connection %i] Invalid status (%i) attempted to set on submid %i",
			 dbc->id, status, submid);
		return 0;
	}

	res = PQexec(dbc->db, sql);
	if( !res ) {
		writelog(dbc->log, LOG_ALERT,
			 "[Connection %i] Unkown error when updating submid %i to status %i",
			 dbc->id, submid, status);
		return -1;
	} else if( PQresultStatus(res) != PGRES_COMMAND_OK ) {
		writelog(dbc->log, LOG_ALERT,
			 "[Connection %i] Failed to UPDATE submissionqueue (submid: %i, status: %i): %s",
			 dbc->id, submid, status, PQresultErrorMessage(res));
		PQclear(res);
		return -1;
	}
	PQclear(res);
	return 1;
}


/**
 * Registers information into the 'systems' and 'systems_hostname' tables, based on the
 * summary/report XML file from rteval.
 *
 * @param dbc        Database handler where to perform the SQL queries
 * @param xslt       A pointer to a parsed 'xmlparser.xsl' XSLT template
 * @param summaryxml The XML report from rteval
 *
 * @return Returns a value > 0 on success, which is a unique reference to the system of the report.
 *         If the function detects that this system is already registered, the 'syskey' reference will
 *         be reused.  On errors, -1 will be returned.
 */
int db_register_system(dbconn *dbc, xsltStylesheet *xslt, xmlDoc *summaryxml) {
	PGresult *dbres = NULL;
	eurephiaVALUES *dbdata = NULL;
	xmlDoc *sysinfo_d = NULL, *hostinfo_d = NULL;
	parseParams prms;
	char sqlq[4098];
	char *sysid = NULL;  // SHA1 value of the system id
	char *ipaddr = NULL, *hostname = NULL;
	int syskey = -1;

	memset(&prms, 0, sizeof(parseParams));
	prms.table = "systems";
	sysinfo_d = parseToSQLdata(dbc->log, xslt, summaryxml, &prms);
	if( !sysinfo_d ) {
		writelog(dbc->log, LOG_ERR, "[Connection %i] Could not parse the input XML data", dbc->id);
		syskey= -1;
		goto exit;
	}
	sysid = sqldataGetValue(dbc->log, sysinfo_d, "sysid", 0);
	if( !sysid ) {
		writelog(dbc->log, LOG_ERR,
			 "[Connection %i] Could not retrieve the sysid field from the input XML", dbc->id);
		syskey= -1;
		goto exit;
	}

	memset(&sqlq, 0, 4098);
	snprintf(sqlq, 4096, "SELECT syskey FROM systems WHERE sysid = '%.256s'", sysid);
	free_nullsafe(sysid);
	dbres = PQexec(dbc->db, sqlq);
	if( PQresultStatus(dbres) != PGRES_TUPLES_OK ) {
		writelog(dbc->log, LOG_ALERT, "[Connection %i] SQL %s",
			 dbc->id, PQresultErrorMessage(dbres));
		writelog(dbc->log, LOG_DEBUG, "[Connection %i] Failing SQL query: %s",
			 dbc->id, sqlq);
		PQclear(dbres);
		syskey= -1;
		goto exit;
	}

	if( PQntuples(dbres) == 0 ) {  // No record found, need to register this system
		PQclear(dbres);

		dbdata = pgsql_INSERT(dbc, sysinfo_d);
		if( !dbdata ) {
			syskey= -1;
			goto exit;
		}
		if( (eCount(dbdata) != 1) || !dbdata->val ) { // Only one record should be registered
			writelog(dbc->log, LOG_ALERT,
				 "[Connection %i] Failed to register the system", dbc->id);
			eFree_values(dbdata);
			syskey= -1;
			goto exit;
		}
		syskey = atoi_nullsafe(dbdata->val);
		hostinfo_d = sqldataGetHostInfo(dbc->log, xslt, summaryxml, syskey, &hostname, &ipaddr);
		if( !hostinfo_d ) {
			syskey = -1;
			goto exit;
		}
		eFree_values(dbdata);

		dbdata = pgsql_INSERT(dbc, hostinfo_d);
		syskey = (dbdata ? syskey : -1);
		eFree_values(dbdata);

	} else if( PQntuples(dbres) == 1 ) { // System found - check if the host IP is known or not
		syskey = atoi_nullsafe(PQgetvalue(dbres, 0, 0));
		hostinfo_d = sqldataGetHostInfo(dbc->log, xslt, summaryxml, syskey, &hostname, &ipaddr);
		if( !hostinfo_d ) {
			syskey = -1;
			goto exit;
		}
		PQclear(dbres);

		// Check if this hostname and IP address is registered
		snprintf(sqlq, 4096,
			 "SELECT syskey FROM systems_hostname"
			 " WHERE hostname='%.256s'",
			 hostname);

		if( ipaddr ) {
			append_str(sqlq, "AND ipaddr='", 4028);
			append_str(sqlq, ipaddr, 4092);
			append_str(sqlq, "'", 4096);
		} else {
			append_str(sqlq, "%s AND ipaddr IS NULL", 4096);
		}

		dbres = PQexec(dbc->db, sqlq);
		if( PQresultStatus(dbres) != PGRES_TUPLES_OK ) {
			writelog(dbc->log, LOG_ALERT, "[Connection %i] SQL %s",
				 dbc->id, PQresultErrorMessage(dbres));
			writelog(dbc->log, LOG_DEBUG, "[Connection %i] Failing SQL query: %s",
				 dbc->id, sqlq);
			PQclear(dbres);
			syskey= -1;
			goto exit;
		}

		if( PQntuples(dbres) == 0 ) { // Not registered, then register it
			dbdata = pgsql_INSERT(dbc, hostinfo_d);
			syskey = (dbdata ? syskey : -1);
			eFree_values(dbdata);
		}
		PQclear(dbres);
	} else {
		// Critical -- system IDs should not be registered more than once
		writelog(dbc->log, LOG_CRIT, "[Connection %i] Multiple systems registered (%s)",
			 dbc->id, sqlq);
		syskey= -1;
	}

 exit:
	free_nullsafe(hostname);
	free_nullsafe(ipaddr);
	if( sysinfo_d ) {
		xmlFreeDoc(sysinfo_d);
	}
	if( hostinfo_d ) {
		xmlFreeDoc(hostinfo_d);
	}
	return syskey;
}


/**
 * Retrieves the next available rteval run ID (rterid)
 *
 * @param dbc  Database handler where to perform the SQL query
 *
 * @return Returns a value > 0 on success, containing the assigned rterid value.  Otherwise -1 is returned.
 */
int db_get_new_rterid(dbconn *dbc) {
	PGresult *dbres = NULL;
	int rterid = 0;

	dbres = PQexec(dbc->db, "SELECT nextval('rtevalruns_rterid_seq')");
	if( (PQresultStatus(dbres) != PGRES_TUPLES_OK) || (PQntuples(dbres) != 1) ) {
		rterid = -1;
	} else {
		rterid = atoi_nullsafe(PQgetvalue(dbres, 0, 0));
	}

	if( rterid < 1 ) {
		writelog(dbc->log, LOG_CRIT,
			 "[Connection %i] Failed to retrieve a new rterid value", dbc->id);
	}
	if( rterid < 0 ) {
		writelog(dbc->log, LOG_ALERT, "[Connection %i] SQL %s",
			 dbc->id, PQresultErrorMessage(dbres));
	}
	PQclear(dbres);
	return rterid;
}


/**
 * Registers information into the 'rtevalruns' and 'rtevalruns_details' tables
 *
 * @param dbc           Database handler where to perform the SQL queries
 * @param xslt          A pointer to a parsed 'xmlparser.xsl' XSLT template
 * @param summaryxml    The XML report from rteval
 * @param submid        Submission ID, referencing the record in the submissionqueue table.
 * @param syskey        A positive integer containing the return value from db_register_system()
 * @param rterid        A positive integer containing the return value from db_get_new_rterid()
 * @param report_fname  A string containing the filename of the report.
 *
 * @return Returns 1 on success, otherwise -1 is returned.
 */
int db_register_rtevalrun(dbconn *dbc, xsltStylesheet *xslt, xmlDoc *summaryxml,
			  unsigned int submid, int syskey, int rterid, const char *report_fname)
{
	int ret = -1;
	xmlDoc *rtevalrun_d = NULL, *rtevalrundets_d = NULL;
	parseParams prms;
	eurephiaVALUES *dbdata = NULL;

	// Parse the rtevalruns information
	memset(&prms, 0, sizeof(parseParams));
	prms.table = "rtevalruns";
	prms.syskey = syskey;
	prms.rterid = rterid;
	prms.submid = submid;
	prms.report_filename = report_fname;
	rtevalrun_d = parseToSQLdata(dbc->log, xslt, summaryxml, &prms);
	if( !rtevalrun_d ) {
		writelog(dbc->log, LOG_ERR,
			 "[Connection %i] Could not parse the input XML data", dbc->id);
		ret = -1;
		goto exit;
	}

	// Register the rteval run information
	dbdata = pgsql_INSERT(dbc, rtevalrun_d);
	if( !dbdata ) {
		ret = -1;
		goto exit;
	}

	if( eCount(dbdata) != 1 ) {
		writelog(dbc->log, LOG_ALERT,
			 "[Connection %i] Failed to register the rteval run", dbc->id);
		ret = -1;
		eFree_values(dbdata);
		goto exit;
	}
	eFree_values(dbdata);

	// Parse the rtevalruns_details information
	memset(&prms, 0, sizeof(parseParams));
	prms.table = "rtevalruns_details";
	prms.rterid = rterid;
	rtevalrundets_d = parseToSQLdata(dbc->log, xslt, summaryxml, &prms);
	if( !rtevalrundets_d ) {
		writelog(dbc->log, LOG_ERR,
			 "[Connection %i] Could not parse the input XML data (rtevalruns_details)",
			 dbc->id);
		ret = -1;
		goto exit;
	}

	// Register the rteval_details information
	dbdata = pgsql_INSERT(dbc, rtevalrundets_d);
	if( !dbdata ) {
		ret = -1;
		goto exit;
	}

	// Check that only one record was inserted
	if( eCount(dbdata) != 1 ) {
		writelog(dbc->log, LOG_ALERT,
			 "[Connection %i] Failed to register the rteval run details", dbc->id);
		ret = -1;
	}
	eFree_values(dbdata);
	ret = 1;
 exit:
	if( rtevalrun_d ) {
		xmlFreeDoc(rtevalrun_d);
	}
	if( rtevalrundets_d ) {
		xmlFreeDoc(rtevalrundets_d);
	}
	return ret;
}


/**
 * Registers data returned from cyclictest into the database.
 *
 * @param dbc      Database handler where to perform the SQL queries
 * @param xslt       A pointer to a parsed 'xmlparser.xsl' XSLT template
 * @param summaryxml The XML report from rteval
 * @param rterid     A positive integer referencing the rteval run ID, returned from db_register_rtevalrun()
 *
 * @return Returns 1 on success, otherwise -1
 */
int db_register_cyclictest(dbconn *dbc, xsltStylesheet *xslt, xmlDoc *summaryxml, int rterid) {
	int result = -1;
	xmlDoc *cyclic_d = NULL;
	parseParams prms;
	eurephiaVALUES *dbdata = NULL;
	int cyclicdata = 0;
	const char *cyclictables[] = { "cyclic_statistics", "cyclic_histogram", "cyclic_rawdata", NULL };
	int i;

	memset(&prms, 0, sizeof(parseParams));
	prms.rterid = rterid;

	// Register the cyclictest data
	for( i = 0; cyclictables[i]; i++ ) {
		prms.table = cyclictables[i];
		cyclic_d = parseToSQLdata(dbc->log, xslt, summaryxml, &prms);
		if( cyclic_d && cyclic_d->children ) {
			// Insert SQL data which was found and generated
			dbdata = pgsql_INSERT(dbc, cyclic_d);
			if( !dbdata ) {
				result = -1;
				xmlFreeDoc(cyclic_d);
				goto exit;
			}

			if (eCount(dbdata) > 0) {
				cyclicdata++;
			}
			eFree_values(dbdata);
			cyclicdata = 1;
		}
		if( cyclic_d ) {
			xmlFreeDoc(cyclic_d);
		}
	}

	// Report error if not enough cyclictest data is registered.
	if( cyclicdata > 1 ) {
		writelog(dbc->log, LOG_ALERT,
			 "[Connection %i] No cyclictest raw data or histogram data registered", dbc->id);
		result = -1;
	} else {
		result = 1;
	}
 exit:
	return result;
}
