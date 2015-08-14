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
 * @file   parsethread.c
 * @author David Sommerseth <davids@redhat.com>
 * @date   Thu Oct 15 11:52:10 2009
 *
 * @brief  Contains the "main" function which a parser threads runs
 *
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <pthread.h>
#include <libgen.h>
#include <errno.h>
#include <assert.h>

#include <eurephia_nullsafe.h>
#include <parsethread.h>
#include <pgsql.h>
#include <log.h>
#include <threadinfo.h>
#include <statuses.h>


/**
 * Does the same job as 'mkdir -p', but it expects a complete filename as input, and it will
 * extract the directory path from that filename
 *
 * @param fname  Full filename containing the directory the report will reside.
 *
 * @return Returns 1 on success, otherwise -1
 */
static int make_report_dir(LogContext *log, const char *fname) {
	char *fname_cp = NULL, *dname = NULL, *chkdir = NULL;
	char *tok = NULL, *saveptr = NULL;
	int ret = 0;
	struct stat info;

	if( !fname ) {
		return 0;
	}

	fname_cp = strdup(fname);
	assert( fname_cp != NULL );
	dname = dirname(fname_cp);
	chkdir = malloc_nullsafe(log, strlen(dname)+8);

	if( dname[0] == '/' ) {
		chkdir[0] = '/';
	}

	// Traverse the directory path, and make sure the directory exists
	tok = strtok_r(dname, "/", &saveptr);
	while( tok ) {
		strcat(chkdir, tok);
		strcat(chkdir, "/");

		errno = 0;
		// Check if directory exists
		if( (stat(chkdir, &info) < 0) ) {
			switch( errno ) {
			case ENOENT: // If the directory do not exist, create it
				if( mkdir(chkdir, 0755) < 0 ) {
					// If creating dir failed, report error
					writelog(log, LOG_ALERT,
						 "Could not create directory: %s (%s)",
						 chkdir, strerror(errno));
					ret = -1;
					goto exit;
				}
				break;
			default: // If other failure, report that and exit
				writelog(log, LOG_ALERT,
					 "Could not access directory: %s (%s)",
					 chkdir, strerror(errno));
				ret = -1;
				goto exit;
			}
		}
		// Goto next path element
		tok = strtok_r(NULL, "/", &saveptr);
	}
	ret = 1;
 exit:
	free_nullsafe(fname_cp);
	free_nullsafe(chkdir);

	return ret;
}


/**
 * Builds up a proper full path of where to save the report.
 *
 * @param destdir   Destination directory for all reports
 * @param fname     Report filename, containing hostname of the reporter
 * @param rterid    rteval run ID
 *
 * @return Returns a pointer to a string with the new full path filename on success, otherwise NULL.
 */
static char *get_destination_path(LogContext *log, const char *destdir,
				  parseJob_t *job, const int rterid)
{
        char *newfname = NULL;
        int retlen = 0;

        if( !job || rterid < 0 ) {
                return NULL;
        }

        retlen = strlen_nullsafe(job->clientid) + strlen(destdir) + 24;
        newfname = malloc_nullsafe(log, retlen+2);

        snprintf(newfname, retlen, "%s/%s/report-%i.xml", destdir, job->clientid, rterid);

        return newfname;
}


/**
 * Checks if the file size of the given file is below the given max size value.
 *
 * @param thrdata  Pointer to a threadData_t structure with log context and max_report_size setting
 * @param fname    Filename of the file to check
 *
 * @return Returns 1 if file is within the limit, otherwise 0.  On errors -1 is returned.
 */
inline int check_filesize(threadData_t *thrdata, const char *fname) {
	struct stat info;

	if( !fname ) {
		return 0;
	}

	errno = 0;
	if( (stat(fname, &info) < 0) ) {
		writelog(thrdata->dbc->log, LOG_ERR, "Failed to check report file '%s': %s",
			 fname, strerror(errno));
		return -1;
	}

	return (info.st_size <= thrdata->max_report_size);
}


/**
 * The core parse function.  Parses an XML file and stores it in the database according to
 * the xmlparser.xsl template.
 *
 * @param thrdata  Pointer to a threadData_t structure with database connection, log context, settings, etc
 * @param job      Pointer to a parseJob_t structure containing the job information
 *
 * @return Return values:
 * @code
 *          STAT_SUCCESS  : Successfully registered report
 *          STAT_FTOOBIG  : XML report file is too big
 *          STAT_XMLFAIL  : Could not parse the XML report file
 *          STAT_SYSREG   : Failed to register the system into the systems or systems_hostname tables
 *          STAT_RTERIDREG: Failed to get a new rterid value
 *          STAT_GENDB    : Failed to start an SQL transaction (BEGIN)
 *          STAT_RTEVRUNS : Failed to register the rteval run into rtevalruns or rtevalruns_details
 *          STAT_CYCLIC   : Failed to register the data into cyclic_statistics or cyclic_rawdata tables
 *          STAT_REPMOVE  : Failed to move the report file
 * @endcode
 */
inline int parse_report(threadData_t *thrdata, parseJob_t *job)
{
	int syskey = -1, rterid = -1;
	int rc = -1;
	xmlDoc *repxml = NULL;
	char *destfname;

	// Check file size - and reject too big files
	if( check_filesize(thrdata, job->filename) == 0 ) {
		writelog(thrdata->dbc->log, LOG_ERR,
			 "[Thread %i] (submid: %i) Report file '%s' is too big, rejected",
			 thrdata->id, job->submid, job->filename);
		return STAT_FTOOBIG;
	}


	repxml = xmlParseFile(job->filename);
	if( !repxml ) {
		writelog(thrdata->dbc->log, LOG_ERR,
			 "[Thread %i] (submid: %i) Could not parse XML file: %s",
			 thrdata->id, job->submid, job->filename);
	        return STAT_XMLFAIL;
	}

	pthread_mutex_lock(thrdata->mtx_sysreg);
	syskey = db_register_system(thrdata->dbc, thrdata->xslt, repxml);
	if( syskey < 0 ) {
		writelog(thrdata->dbc->log, LOG_ERR,
			 "[Thread %i] Failed to register system (submid: %i, XML file: %s)",
			 thrdata->id, job->submid, job->filename);
		rc = STAT_SYSREG;
		goto exit;

	}
	rterid = db_get_new_rterid(thrdata->dbc);
	if( rterid < 0 ) {
		writelog(thrdata->dbc->log, LOG_ERR,
			 "[Thread %i] Failed to register rteval run (submid: %i, XML file: %s)",
			 thrdata->id, job->submid, job->filename);
		rc = STAT_RTERIDREG;
		goto exit;
	}
	pthread_mutex_unlock(thrdata->mtx_sysreg);

	if( db_begin(thrdata->dbc) < 1 ) {
		rc = STAT_GENDB;
		goto exit;
	}

	// Create a new filename of where to save the report
	destfname = get_destination_path(thrdata->dbc->log, thrdata->destdir, job, rterid);
	if( !destfname ) {
		writelog(thrdata->dbc->log, LOG_ERR,
			 "[Thread %i] Failed to generate local report filename for (submid: %i) %s",
			 thrdata->id, job->submid, job->filename);
		db_rollback(thrdata->dbc);
		rc = STAT_UNKNFAIL;
		goto exit;
	}

	if( db_register_rtevalrun(thrdata->dbc, thrdata->xslt, repxml, job->submid,
				  syskey, rterid, destfname) < 0 ) {
		writelog(thrdata->dbc->log, LOG_ERR,
			 "[Thread %i] Failed to register rteval run (submid: %i, XML file: %s)",
			 thrdata->id, job->submid, job->filename);
		db_rollback(thrdata->dbc);
		rc = STAT_RTEVRUNS;
		goto exit;
	}

	if( db_register_cyclictest(thrdata->dbc, thrdata->xslt, repxml, rterid) != 1 ) {
		writelog(thrdata->dbc->log, LOG_ERR,
			 "[Thread %i] Failed to register cyclictest data (submid: %i, XML file: %s)",
			 thrdata->id, job->submid, job->filename);
		db_rollback(thrdata->dbc);
		rc = STAT_CYCLIC;
		goto exit;
	}

	// When all database registrations are done, move the file to it's right place
	if( make_report_dir(thrdata->dbc->log, destfname) < 1 ) { // Make sure report directory exists
		db_rollback(thrdata->dbc);
		rc = STAT_REPMOVE;
		goto exit;
	}

	if( rename(job->filename, destfname) < 0 ) { // Move the file
		writelog(thrdata->dbc->log, LOG_ERR,
			 "[Thread %i] (submid: %i) Failed to move report file from %s to %s (%s)",
			 thrdata->id, job->submid, job->filename, destfname, strerror(errno));
		db_rollback(thrdata->dbc);
		rc = STAT_REPMOVE;
		goto exit;
	}
	free_nullsafe(destfname);

	rc = STAT_SUCCESS;
	db_commit(thrdata->dbc);
	writelog(thrdata->dbc->log, LOG_INFO,
		 "[Thread %i] Report parsed and stored (submid: %i, rterid: %i)",
		 thrdata->id, job->submid, rterid);
 exit:
	xmlFreeDoc(repxml);
	return rc;
}


/**
 * The parser thread.  This thread lives until a shutdown notification is received.  It pulls
 * messages on a POSIX MQ based message queue containing submission ID and full path to an XML
 * report to be parsed.
 *
 * @param thrargs Contains database connection, XSLT stylesheet, POSXI MQ descriptor, etc
 *
 * @return Returns 0 on successful operation, otherwise 1 on errors.
 */
void *parsethread(void *thrargs) {
	threadData_t *args = (threadData_t *) thrargs;
	parseJob_t jobinfo;
	long exitcode = 0;

	writelog(args->dbc->log, LOG_DEBUG, "[Thread %i] Starting", args->id);
	pthread_mutex_lock(args->mtx_thrcnt);
	(*(args->threadcount)) += 1;
	pthread_mutex_unlock(args->mtx_thrcnt);

	// Polling loop
	while( *(args->shutdown) == 0 ) {
		int len = 0;
		unsigned int prio = 0;

		// Check if the database connection is alive before pulling any messages
		if( db_ping(args->dbc) != 1 ) {
			writelog(args->dbc->log, LOG_EMERG,
				 "[Thread %i] Lost database conneciting: Shutting down thread.",
				 args->id);

			if( *(args->threadcount) <= 1 ) {
				writelog(args->dbc->log, LOG_EMERG,
					 "No more worker threads available.  "
					 "Signaling for complete shutdown!");
				kill(getpid(), SIGUSR1);
			}
			exitcode = 1;
			goto exit;
		}

		// Retrieve a parse job from the message queue
		memset(&jobinfo, 0, sizeof(parseJob_t));
		errno = 0;
		len = mq_receive(args->msgq, (char *)&jobinfo, sizeof(parseJob_t), &prio);
		if( (len < 0) && errno != EAGAIN ) {
			writelog(args->dbc->log, LOG_CRIT,
				 "Could not receive the message from queue: %s",
				 strerror(errno));
			pthread_exit((void *) 1);
		}

		// Ignore whatever message if the shutdown flag is set.
		if( *(args->shutdown) != 0 ) {
			break;
		}

		// If we have a message, then process the parse job
		if( (errno != EAGAIN) && (len > 0) ) {
			int res = 0;

			writelog(args->dbc->log, LOG_INFO,
				 "[Thread %i] Job recieved, submid: %i - %s",
				 args->id, jobinfo.submid, jobinfo.filename);

			// Mark the job as "in progress", if successful update, continue parsing it
			if( db_update_submissionqueue(args->dbc, jobinfo.submid, STAT_INPROG) ) {
				res = parse_report(args, &jobinfo);
				// Set the status for the submission
				db_update_submissionqueue(args->dbc, jobinfo.submid, res);
			} else {
				writelog(args->dbc->log, LOG_CRIT,
					 "Failed to mark submid %i as STAT_INPROG",
					 jobinfo.submid);
			}
		}
	}
	writelog(args->dbc->log, LOG_DEBUG, "[Thread %i] Shut down", args->id);
 exit:
	pthread_mutex_lock(args->mtx_thrcnt);
	(*(args->threadcount)) -= 1;
	pthread_mutex_unlock(args->mtx_thrcnt);

	pthread_exit((void *) exitcode);
}
