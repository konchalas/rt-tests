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
 * @file   argparser.c
 * @author David Sommerseth <davids@redhat.com>
 * @date   Thu Oct 22 13:58:46 2009
 *
 * @brief  Generic argument parser
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <eurephia_values.h>
#include <eurephia_nullsafe.h>


/**
 * Print a help screen to stdout
 */
void usage() {
	printf("rteval-parserd:  Parses new reports recieved via XML-RPC\n"
	       "\n"
	       "This program will wait for changes to the rteval 'submissionqueue' table.\n"
	       "When a new report is registered here, it will send this report to one of\n"
	       "the worker threads which will insert the parsed result into the database.\n"
	       "\n"
	       "** Program arguments:\n"
	       "  -d | --daemon                    Run as a daemon\n"
	       "  -l | --log        <log dest>     Where to put log data\n"
	       "  -L | --log-level  <verbosity>    What to log\n"
	       "  -f | --config     <config file>  Which configuration file to use\n"
	       "  -t | --threads    <num. threads> How many worker threads to start (def: 4)\n"
	       "  -h | --help                      This help screen\n"
	       "\n"
	       "** Configuration file\n"
	       "By default the program will look for /etc/rteval.conf.  This can be\n"
	       "overriden by using --config <config file>.\n"
	       "\n"
	       "** Logging\n"
	       "When the program is started as a daemon, it will log to syslog by default.\n"
	       "The default log level is 'info'.  When not started as a daemon, all logging\n"
	       "will go to stderr by default.\n"
	       "\n"
	       "The --log argument takes either 'destination' or a file name.  Unknown\n"
	       "destinations are treated as filenames.  Valid 'destinations' are:\n"
	       "\n"
	       "    stderr:             - Log to stderr\n"
	       "    stdout:             - Log to stdout\n"
	       "    syslog:[facility]   - Log to syslog\n"
	       "    <file name>         - Log to given file\n"
	       "\n"
	       "For syslog the default facility is 'daemon', but can be overriden by using\n"
	       "one of the following facility values:\n"
	       "    daemon, user and local0 to local7\n"
	       "\n"
	       "Log verbosity is set by the --log-level.  The valid values here are:\n"
	       "\n"
	       "    emerg, emergency    - Only log errors which causes the program to stop\n"
	       "    alert               - Incidents which needs immediate attention\n"
	       "    crit, critical      - Unexpected incidents which is not urgent\n"
	       "    err, error          - Parsing errors.  Issues with input data\n"
	       "    warn, warning       - Incidents which may influence performance\n"
	       "    notice              - Less important warnings\n"
	       "    info                - General run information\n"
	       "    debug               - Detailed run information, incl. thread operations\n"
	       "\n"
	       );
}


/**
 * Parses program arguments and puts the recognised arguments into an eurephiaVALUES struct.
 *
 * @param argc   argument counter
 * @param argv   argument string table
 *
 * @return Returns a pointer to an eurephiaVALUES struct.  On failure, the program halts.
 */
eurephiaVALUES *parse_arguments(int argc, char **argv) {
	eurephiaVALUES *args = NULL;
	int optidx, c;
	static struct option long_opts[] = {
		{"log", 1, 0, 'l'},
		{"log-level", 1, 0, 'L'},
		{"config", 1, 0, 'f'},
		{"threads", 1, 0, 't'},
		{"daemon", 0, 0, 'd'},
		{"help", 0, 0, 'h'},
		{0, 0, 0, 0}
	};

	args = eCreate_value_space(NULL, 21);
	eAdd_value(args, "daemon", "0");
	eAdd_value(args, "configfile", "/etc/rteval.conf");
	eAdd_value(args, "threads", "4");

	while( 1 ) {
		optidx = 0;
		c = getopt_long(argc, argv, "l:L:f:t:dh", long_opts, &optidx);
		if( c == -1 ) {
			break;
		}

		switch( c ) {
		case 'l':
			eUpdate_value(args, "log", optarg, 1);
			break;
		case 'L':
			eUpdate_value(args, "loglevel", optarg, 1);
			break;
		case 'f':
			eUpdate_value(args, "configfile", optarg, 0);
			break;
		case 't':
			eUpdate_value(args, "threads", optarg, 0);
			break;
		case 'd':
			eUpdate_value(args, "daemon", "1", 0);
			break;
		case 'h':
			usage();
			exit(0);
		}
	}

	// If logging is not configured, and it is not run as a daemon
	// -> log to stderr:
	if( (eGet_value(args, "log") == NULL)
	    && (atoi_nullsafe(eGet_value(args, "daemon")) == 0) ) {
		eAdd_value(args, "log", "stderr:");
	}

	return args;
}
