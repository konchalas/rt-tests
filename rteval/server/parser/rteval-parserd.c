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
 * @file   rteval-parserd.c
 * @author David Sommerseth <davids@redhat.com>
 * @date   Thu Oct 15 11:59:27 2009
 *
 * @brief  Polls the rteval.submissionqueue table for notifications
 *         from new inserts and sends the file to a processing thread
 *
 *
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>

#include <eurephia_nullsafe.h>
#include <eurephia_values.h>
#include <configparser.h>
#include <pgsql.h>
#include <threadinfo.h>
#include <parsethread.h>
#include <argparser.h>

#define DEFAULT_MSG_MAX 5             /**< Default size of the message queue */
#define XMLPARSER_XSL "xmlparser.xsl" /**< rteval report parser XSLT, parses XML into database friendly data*/

static int shutdown = 0;              /**<  Variable indicating if the program should shutdown */
static LogContext *logctx = NULL;     /**<  Initialsed log context, to be used by sigcatch() */


/**
 * Simple signal catcher.  Used for SIGINT and SIGTERM signals, and will set the global shutdown
 * shutdown flag.  It's expected that all threads behaves properly and exits as soon as their current
 * work is completed
 *
 * @param sig Recieved signal (not used)
 */
void sigcatch(int sig) {
	switch( sig ) {
	case SIGINT:
	case SIGTERM:
		if( shutdown == 0 ) {
			shutdown = 1;
			writelog(logctx, LOG_INFO, "[SIGNAL] Shutting down");
		} else {
			writelog(logctx, LOG_INFO, "[SIGNAL] Shutdown in progress ... please be patient ...");
		}
		break;

	case SIGUSR1:
		writelog(logctx, LOG_EMERG, "[SIGNAL] Shutdown alarm from a worker thread");
		shutdown = 1;
		break;

	default:
		break;
	}

	// re-enable signals, to avoid brute force exits.
	// If brute force is needed, SIGKILL is available.
	signal(sig, sigcatch);
}


/**
 * Opens and reads /proc/sys/fs/mqueue/msg_max, to get the maximum number of allowed messages
 * on POSIX MQ queues.  rteval-parserd will use as much of this as possible when needed.
 *
 * @return Returns the system msg_max value, or DEFAULT_MSG_MAX on failure to read the setting.
 */
unsigned int get_mqueue_msg_max(LogContext *log) {
	FILE *fp = NULL;
	char buf[130];
	unsigned int msg_max = DEFAULT_MSG_MAX;

	fp = fopen("/proc/sys/fs/mqueue/msg_max", "r");
	if( !fp ) {
		writelog(log, LOG_WARNING,
			"Could not open /proc/sys/fs/mqueue/msg_max, defaulting to %i",
			msg_max);
		writelog(log, LOG_INFO, "%s", strerror(errno));
		return msg_max;
	}

	memset(&buf, 0, 130);
	if( fread(&buf, 1, 128, fp) < 1 ) {
		writelog(log, LOG_WARNING,
			"Could not read /proc/sys/fs/mqueue/msg_max, defaulting to %i",
			msg_max);
		writelog(log, LOG_INFO, "%s", strerror(errno));
	} else {
		msg_max = atoi_nullsafe(buf);
		if( msg_max < 1 ) {
			msg_max = DEFAULT_MSG_MAX;
			writelog(log, LOG_WARNING,
				"Failed to parse /proc/sys/fs/mqueue/msg_max,"
				"defaulting to %i", msg_max);
		}
	}
	fclose(fp);
	return msg_max;
}


/**
 * Main loop, which polls the submissionqueue table and puts jobs found here into a POSIX MQ queue
 * which the worker threads will pick up.
 *
 * @param dbc           Database connection, where to query the submission queue
 * @param msgq          file descriptor for the message queue
 * @param activethreads Pointer to an int value containing active worker threads.  Each thread updates
 *                      this value directly, and this function should only read it.
 *
 * @return Returns 0 on successful run, otherwise > 0 on errors.
 */
int process_submission_queue(dbconn *dbc, mqd_t msgq, int *activethreads) {
	pthread_mutex_t mtx_submq = PTHREAD_MUTEX_INITIALIZER;
	parseJob_t *job = NULL;
	int rc = 0, i, actthr_cp = 0;

	while( shutdown == 0 ) {
		// Check status if the worker threads
		// If we don't have any worker threads, shut down immediately
		writelog(dbc->log, LOG_DEBUG, "Active worker threads: %i", *activethreads);
		if( *activethreads < 1 ) {
			writelog(dbc->log, LOG_EMERG,
				 "All worker threads ceased to exist.  Shutting down!");
			shutdown = 1;
			rc = 1;
			goto exit;
		}

		if( db_ping(dbc) != 1 ) {
			writelog(dbc->log, LOG_EMERG, "Lost connection to database.  Shutting down!");
			shutdown = 1;
			rc = 1;
			goto exit;
		}

		// Fetch an available job
		job = db_get_submissionqueue_job(dbc, &mtx_submq);
		if( !job ) {
			writelog(dbc->log, LOG_EMERG,
				 "Failed to get submission queue job.  Shutting down!");
			shutdown = 1;
			rc = 1;
			goto exit;
		}
		if( job->status == jbNONE ) {
			free_nullsafe(job);
			if( db_wait_notification(dbc, &shutdown, "rteval_submq") < 1 ) {
				writelog(dbc->log, LOG_EMERG,
					 "Failed to wait for DB notification.  Shutting down!");
				shutdown = 1;
				rc = 1;
				goto exit;
			}
			continue;
		}

		// Send the job to the queue
		writelog(dbc->log, LOG_DEBUG, "** New job queued: submid %i, %s", job->submid, job->filename);
		do {
			int res;

			errno = 0;
			res = mq_send(msgq, (char *) job, sizeof(parseJob_t), 1);
			if( (res < 0) && (errno != EAGAIN) ) {
				writelog(dbc->log, LOG_EMERG,
					 "Could not send parse job to the queue.  "
					 "Shutting down!");
				shutdown = 1;
				rc = 2;
				goto exit;
			} else if( errno == EAGAIN ) {
				writelog(dbc->log, LOG_WARNING,
					"Message queue filled up.  "
					"Will not add new messages to queue for the next 60 seconds");
				sleep(60);
			}
		} while( (errno == EAGAIN) );
		free_nullsafe(job);
	}

 exit:
	// Send empty messages to the threads, to make them have a look at the shutdown flag
	job = (parseJob_t *) malloc_nullsafe(dbc->log, sizeof(parseJob_t));
	errno = 0;
	// Need to make a copy, as *activethreads will change when threads completes shutdown
	actthr_cp = *activethreads;
	for( i = 0; i < actthr_cp; i++ ) {
		do {
			int res;

			writelog(dbc->log, LOG_DEBUG, "%s shutdown message %i of %i",
				 (errno == EAGAIN ? "Resending" : "Sending"), i+1, *activethreads);
			errno = 0;
			res = mq_send(msgq, (char *) job, sizeof(parseJob_t), 1);
			if( (res < 0) && (errno != EAGAIN) ) {
				writelog(dbc->log, LOG_EMERG,
					 "Could not send shutdown notification to the queue.");
				free_nullsafe(job);
				return rc;
			} else if( errno == EAGAIN ) {
				writelog(dbc->log, LOG_WARNING,
					"Message queue filled up.  "
					"Will not add new messages to queue for the next 10 seconds");
				sleep(10);
			}
		} while( (errno == EAGAIN) );
	}
	free_nullsafe(job);
	return rc;
}


/**
 * Prepares the program to be daemonised
 *
 * @param log   Initialised log context, where log info of the process is reported
 *
 * @return Returns 1 on success, otherwise -1
 */
int daemonise(LogContext *log) {
	pid_t pid, sid;
	int i = 0;

	if( (log->logtype == ltCONSOLE) ) {
		writelog(log, LOG_EMERG,
			 "Cannot daemonise when logging to a console (stdout: or stderr:)");
		return -1;
	}

	pid = fork();
	if (pid < 0) {
		writelog(log, LOG_EMERG, "Failed to daemonise the process (fork)");
		return -1;
	} else if (pid > 0) {
		writelog(log, LOG_INFO, "Daemon pid: %ld", pid);
		exit(EXIT_SUCCESS);
	}

	umask(0);

	sid = setsid();
	if (sid < 0) {
		writelog(log, LOG_EMERG, "Failed to daemonise the process (setsid)");
		return -1;
	}

	if ((chdir("/")) < 0) {
		writelog(log, LOG_EMERG, "Failed to daemonise the process (fork)");
		return -1;
	}

	// Prepare stdin, stdout and stderr for daemon mode
	close(2);
	close(1);
	close(0);
	i = open("/dev/null", O_RDWR); /* open stdin */
	dup(i); /* stdout */
	dup(i); /* stderr */

	writelog(log, LOG_INFO, "Daemonised successfully");
	return 1;
}


/**
 * rtevald_parser main function.
 *
 * @param argc
 * @param argv
 *
 * @return Returns the result of the process_submission_queue() function.
 */
int main(int argc, char **argv) {
        eurephiaVALUES *config = NULL, *prgargs = NULL;
        char xsltfile[2050], *reportdir = NULL;
	xsltStylesheet *xslt = NULL;
	dbconn *dbc = NULL;
        pthread_t **threads = NULL;
        pthread_attr_t **thread_attrs = NULL;
	pthread_mutex_t mtx_sysreg = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_t mtx_thrcnt = PTHREAD_MUTEX_INITIALIZER;
	threadData_t **thrdata = NULL;
	struct mq_attr msgq_attr;
	mqd_t msgq = 0;
	int i,rc, mq_init = 0, max_threads = 0, started_threads = 0, activethreads = 0;
	unsigned int max_report_size = 0;

	// Initialise XML and XSLT libraries
	xsltInit();
	xmlInitParser();

	prgargs = parse_arguments(argc, argv);
	if( prgargs == NULL ) {
		fprintf(stderr, "** ERROR **  Failed to parse program arguments\n");
		rc = 2;
		goto exit;
	}

	// Setup a log context
	logctx = init_log(eGet_value(prgargs, "log"), eGet_value(prgargs, "loglevel"));
	if( !logctx ) {
		fprintf(stderr, "** ERROR **  Could not setup a log context\n");
		eFree_values(prgargs);
		rc = 2;
		goto exit;
	}

	// Fetch configuration
        config = read_config(logctx, prgargs, "xmlrpc_parser");
	eFree_values(prgargs); // read_config() copies prgargs into config, we don't need prgargs anymore

	// Daemonise process if requested
	if( atoi_nullsafe(eGet_value(config, "daemon")) == 1 ) {
		if( daemonise(logctx) < 1 ) {
			rc = 3;
			goto exit;
		}
	}


	// Parse XSLT template
	snprintf(xsltfile, 512, "%s/%s", eGet_value(config, "xsltpath"), XMLPARSER_XSL);
	writelog(logctx, LOG_DEBUG, "Parsing XSLT file: %s", xsltfile);
        xslt = xsltParseStylesheetFile((xmlChar *) xsltfile);
	if( !xslt ) {
		writelog(logctx, LOG_EMERG, "Could not parse XSLT template: %s", xsltfile);
		rc = 2;
		goto exit;
	}

	// Open a POSIX MQ
	writelog(logctx, LOG_DEBUG, "Preparing POSIX MQ queue: /rteval_parsequeue");
	memset(&msgq, 0, sizeof(mqd_t));
	msgq_attr.mq_maxmsg = get_mqueue_msg_max(logctx);
	msgq_attr.mq_msgsize = sizeof(parseJob_t);
	msgq_attr.mq_flags = O_NONBLOCK;
	msgq = mq_open("/rteval_parsequeue", O_RDWR | O_CREAT, 0600, &msgq_attr);
	if( msgq < 0 ) {
		writelog(logctx, LOG_EMERG,
			 "Could not open message queue: %s", strerror(errno));
		rc = 2;
		goto exit;
	}
	mq_init = 1;

	// Get the number of worker threads
	max_threads = atoi_nullsafe(eGet_value(config, "threads"));
	if( max_threads == 0 ) {
		max_threads = 4;
	}

	// Get a database connection for the main thread
        dbc = db_connect(config, max_threads, logctx);
        if( !dbc ) {
		rc = 4;
		goto exit;
        }

	// Prepare all threads
	threads = calloc(max_threads + 1, sizeof(pthread_t *));
	thread_attrs = calloc(max_threads + 1, sizeof(pthread_attr_t *));
	thrdata = calloc(max_threads + 1, sizeof(threadData_t *));
	assert( (threads != NULL) && (thread_attrs != NULL) && (thrdata != NULL) );

	reportdir = eGet_value(config, "reportdir");
	writelog(logctx, LOG_INFO, "Starting %i worker threads", max_threads);
	max_report_size = defaultIntValue(atoi_nullsafe(eGet_value(config, "max_report_size")), 1024*1024);
	for( i = 0; i < max_threads; i++ ) {
		// Prepare thread specific data
		thrdata[i] = malloc_nullsafe(logctx, sizeof(threadData_t));
		if( !thrdata[i] ) {
			writelog(logctx, LOG_EMERG,
				 "Could not allocate memory for thread data");
			rc = 2;
			goto exit;
		}

		// Get a database connection for the thread
		thrdata[i]->dbc = db_connect(config, i, logctx);
		if( !thrdata[i]->dbc ) {
			writelog(logctx, LOG_EMERG,
				"Could not connect to the database for thread %i", i);
			rc = 2;
			shutdown = 1;
			goto exit;
		}

		thrdata[i]->shutdown = &shutdown;
		thrdata[i]->threadcount = &activethreads;
		thrdata[i]->mtx_thrcnt = &mtx_thrcnt;
		thrdata[i]->id = i;
		thrdata[i]->msgq = msgq;
		thrdata[i]->mtx_sysreg = &mtx_sysreg;
		thrdata[i]->xslt = xslt;
		thrdata[i]->destdir = reportdir;
		thrdata[i]->max_report_size = max_report_size;

		thread_attrs[i] = malloc_nullsafe(logctx, sizeof(pthread_attr_t));
		if( !thread_attrs[i] ) {
			writelog(logctx, LOG_EMERG,
				"Could not allocate memory for thread attributes");
			rc = 2;
			goto exit;
		}
		pthread_attr_init(thread_attrs[i]);
		pthread_attr_setdetachstate(thread_attrs[i], PTHREAD_CREATE_JOINABLE);

		threads[i] = malloc_nullsafe(logctx, sizeof(pthread_t));
		if( !threads[i] ) {
			writelog(logctx, LOG_EMERG,
				"Could not allocate memory for pthread_t");
			rc = 2;
			goto exit;
		}
	}

	// Setup signal catching
	signal(SIGINT,  sigcatch);
	signal(SIGTERM, sigcatch);
	signal(SIGHUP,  SIG_IGN);
	signal(SIGUSR1, sigcatch);
	signal(SIGUSR2, SIG_IGN);

	// Start the threads
	for( i = 0; i < max_threads; i++ ) {
		int thr_rc = pthread_create(threads[i], thread_attrs[i], parsethread, thrdata[i]);
		if( thr_rc < 0 ) {
			writelog(logctx, LOG_EMERG,
				 "** ERROR **  Failed to start thread %i: %s",
				 i, strerror(thr_rc));
			rc = 3;
			goto exit;
		}
		started_threads++;
	}

	// Main routine
	//
	// checks the submission queue and puts unprocessed records on the POSIX MQ
	// to be parsed by one of the threads
	//
	sleep(3); // Allow at least a few parser threads to settle down first before really starting
	writelog(logctx, LOG_DEBUG, "Starting submission queue checker");
	rc = process_submission_queue(dbc, msgq, &activethreads);
	writelog(logctx, LOG_DEBUG, "Submission queue checker shut down");

 exit:
	// Clean up all threads
	for( i = 0; i < max_threads; i++ ) {
		// Wait for all threads to exit
		if( (i < started_threads) && threads && threads[i] ) {
			void *thread_rc;
			int j_rc;

			if( (j_rc = pthread_join(*threads[i], &thread_rc)) != 0 ) {
				writelog(logctx, LOG_CRIT,
					 "Failed to join thread %i: %s",
					 i, strerror(j_rc));
			}
			pthread_attr_destroy(thread_attrs[i]);
		}
		if( threads ) {
			free_nullsafe(threads[i]);
		}
		if( thread_attrs ) {
			free_nullsafe(thread_attrs[i]);
		}

		// Disconnect threads database connection
		if( thrdata && thrdata[i] ) {
			db_disconnect(thrdata[i]->dbc);
			free_nullsafe(thrdata[i]);
		}
	}
	free_nullsafe(thrdata);
	free_nullsafe(threads);
	free_nullsafe(thread_attrs);

	// Close message queue
	if( mq_init == 1 ) {
		errno = 0;
		if( mq_close(msgq) < 0 ) {
			writelog(logctx, LOG_CRIT, "Failed to close message queue: %s",
				 strerror(errno));
		}
		errno = 0;
		if( mq_unlink("/rteval_parsequeue") < 0 ) {
			writelog(logctx, LOG_ALERT, "Failed to remove the message queue: %s",
				 strerror(errno));
		}
	}

	// Disconnect from database, main thread connection
	db_disconnect(dbc);

	// Free up the rest
	eFree_values(config);
	xsltFreeStylesheet(xslt);
	xmlCleanupParser();
	xsltCleanupGlobals();

	writelog(logctx, LOG_EMERG, "rteval-parserd is stopped");
	close_log(logctx);
	return rc;
}

