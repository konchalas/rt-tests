/* eurephia_nullsafe.h
 *
 *  standard C string functions, which is made NULL safe by checking
 *  if input value is NULL before performing the action.
 *
 *  This version is modified to work outside the eurephia project.
 *
 *  GPLv2 only - Copyright (C) 2008, 2009
 *               David Sommerseth <dazo@users.sourceforge.net>
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
 * @file   eurephia_nullsafe.h
 * @author David Sommerseth <dazo@users.sourceforge.net>
 * @date   2008-08-06
 *
 * @brief standard C string functions, which is made NULL safe by checking
 *        if input value is NULL before performing the action.
 *
 */

#ifndef   	EUREPHIA_NULLSAFE_H_
#define    	EUREPHIA_NULLSAFE_H_

#include <log.h>

/**
 * atoi() wrapper.  Converts any string into a integer
 *
 * @param str Input string
 *
 * @return Returns integer
 */
#define atoi_nullsafe(str) (str != NULL ? atoi(str) : 0)


/**
 * strdup() wrapper.  Duplicates the input string.
 *
 * @param str Input string to be duplicated
 *
 * @return Returns a pointer to the duplicate (char *) on success, NULL otherwise.
 * If input was NULL, NULL is returned.
 */
#define strdup_nullsafe(str) (str != NULL ? strdup(str) : NULL)


/**
 * Wrapper macro, which appends a string to a destination string without exceeding the size
 * of the destination buffer.
 *
 * @param dest Pointer to the destination buffer
 * @param src  Pointer to the value being concatenated to the destination string.
 * @param size Size of the destination buffer
 */
#define append_str(dest, src, size) strncat(dest, src, (size - strlen_nullsafe(dest)))


/**
 * strlen() wrapper.  Returns the length of a string
 *
 * @param str Input string
 *
 * @return Returns int with length of string.  If input is NULL, it returns 0.
 */
#define strlen_nullsafe(str) (str != NULL ? strlen(str) : 0)


void *malloc_nullsafe(LogContext *, size_t);

/**
 * Null safe free().  It will not attempt to free a pointer which is NULL.
 *
 * @param ptr Pointer to the memory region being freed.
 *
 */
#define free_nullsafe(ptr) if( ptr ) { free(ptr); ptr = NULL; }


/**
 * Function which will return a default string value if no input data was provided.
 *
 * @param str     Input string
 * @param defstr  Default string
 *
 * @return Returns the pointer to the input string if the string length > 0.  Otherwise it
 * will return a pointer to the default string.
 */
#define defaultValue(str, defstr) (strlen_nullsafe(str) == 0 ? defstr : str)


/**
 * Function which will return a default integer value if no input data was provided.
 *
 * @param ival   input integer value
 * @param defval default integer value
 *
 * @return Returns the ival value if it is > 0, otherwise defval value is returned.
 */
#define defaultIntValue(ival, defval) (ival == 0 ? defval : ival)
#endif 	    /* !EUREPHIA_NULLSAFE_H_ */
