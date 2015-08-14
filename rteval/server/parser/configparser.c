/*  configparser.c - Read and parse config files
 *
 *  This code is based on the fragments from the eurephia project.
 *
 *  GPLv2 Copyright (C) 2009
 *  David Sommerseth <davids@redhat.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; version 2
 *  of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

/**
 * @file   configparser.c
 * @author David Sommerseth <davids@redhat.com>
 * @date   2009-10-01
 *
 * @brief  Config file parser
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>

#include <eurephia_nullsafe.h>
#include <eurephia_values.h>
#include <configparser.h>
#include <log.h>

/**
 * Parse one single configuration line into a eurephiaVALUES key/value pair.  It will also ignore
 * comment lines, and also remove the comments on the line of the configuration line so that only
 * the key/value information is extracted.
 *
 * @param line Input configuration line
 *
 * @return eurephiaVALUES pointer containing the parsed result.  On error or if no valid config
 * line was found, NULL is returned.
 */
static inline eurephiaVALUES *parse_config_line(LogContext *log, const char *line) {
        char *cp = NULL, *key = NULL, *val = NULL, *ptr = NULL;;
        eurephiaVALUES *ret = NULL;

        if( *line == '#' ) {
                return NULL;
        }

        cp = strdup(line);
        key = cp;
        val = strpbrk(cp, "=:");
        if( val == NULL ) {
                free_nullsafe(cp);
                return NULL;
        }
        *val = '\0'; val++;

        // Discard comments at the end of a line
        if( (ptr = strpbrk(val, "#")) != NULL ) {
                *ptr = '\0';
        }

        // Left trim
        while( ((*key == 0x20) || (*key == 0x0A) || (*key == 0x0D)) ) {
                key++;
        }
        while( ((*val == 0x20) || (*val == 0x0A) || (*val == 0x0D)) ) {
                val++;
        }

        // Right trim
        ptr = key + strlen_nullsafe(key) - 1;
        while( ((*ptr == 0x20) || (*ptr == 0x0A) || (*ptr == 0x0D)) && (ptr > key) ) {
                ptr--;
        }
        ptr++;
        *ptr = '\0';

        ptr = val + strlen_nullsafe(val) - 1;
        while( ((*ptr == 0x20) || (*ptr == 0x0A) || (*ptr == 0x0D)) && (ptr > val) ) {
                ptr--;
        }
        ptr++;
        *ptr = '\0';

        // Put key/value into a eurephiaVALUES struct and return it
        ret = eCreate_value_space(log, 20);
        ret->key = strdup(key);
        ret->val = strdup(val);

        free_nullsafe(cp);
        return ret;
}


static inline eurephiaVALUES *default_cfg_values(LogContext *log, eurephiaVALUES *prgargs) {
	eurephiaVALUES *cfg = NULL, *ptr = NULL;

	cfg = eCreate_value_space(log, 20);
	eAdd_value(cfg, "datadir", "/var/lib/rteval");
	eAdd_value(cfg, "xsltpath", "/usr/share/rteval");
	eAdd_value(cfg, "db_server", "localhost");
	eAdd_value(cfg, "db_port", "5432");
	eAdd_value(cfg, "database", "rteval");
	eAdd_value(cfg, "db_username", "rtevparser");
	eAdd_value(cfg, "db_password", "rtevaldb_parser");
	eAdd_value(cfg, "reportdir", "/var/lib/rteval/reports");
	eAdd_value(cfg, "max_report_size", "2097152"); // 2MB

	// Copy over the arguments to the config, update existing settings
	for( ptr = prgargs; ptr; ptr = ptr->next ) {
		eUpdate_value(cfg, ptr->key, ptr->val, 1);
	}

	return cfg;
}

/**
 * Parses a section of a config file and puts it into an eurephiaVALUES key/value stack
 *
 * @param log     Initialised log context
 * @param prgargs Parsed command line arguments (see parse_arguments())
 * @param section Section to read from the config file
 *
 * @return Returns a pointer to an eurephiaVALUES stack containing the configuration on success,
 *         otherwise NULL.
 */
eurephiaVALUES *read_config(LogContext *log, eurephiaVALUES *prgargs, const char *section) {
        FILE *fp = NULL;
        char  *buf = NULL, *sectmatch = NULL, *cfgname = NULL;
	int sectfound = 0;
        eurephiaVALUES *cfg = NULL;
        struct stat fi;

	cfgname = eGet_value(prgargs, "configfile");
        if( stat(cfgname, &fi) == -1 ) {
		writelog(log, LOG_EMERG, "Could not open the config file: %s", cfgname);
                return NULL;
        }

        if( (fp = fopen(cfgname, "r")) == NULL ) {
                writelog(log, LOG_EMERG, "Could not open the config file: %s", cfgname);
                return NULL;
        }

        buf = (char *) malloc_nullsafe(log, fi.st_size+2);
	sectmatch = (char *) malloc_nullsafe(log, strlen_nullsafe(section)+4);
	sprintf(sectmatch, "[%s]", section);

        cfg = default_cfg_values(log, prgargs);
	writelog(log, LOG_DEBUG, "Reading config file: %s", cfgname);
        while( fgets(buf, fi.st_size, fp) != NULL ) {
		if( strncmp(buf, "[", 1) == 0 ) {
			sectfound = (!sectfound && (strncmp(buf, sectmatch, strlen(sectmatch)) == 0));
			continue;
		}

		if( sectfound ) {
			eurephiaVALUES *prm = parse_config_line(log, buf);
			if( prm != NULL ) {
				cfg = eUpdate_valuestruct(cfg, prm, 1);
			}
		}
        };
        free_nullsafe(buf);
	free_nullsafe(sectmatch);
        fclose(fp); fp = NULL;

        return cfg;
}
