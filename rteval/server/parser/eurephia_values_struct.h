/* eurephia_values.h  --  eurephiaVALUES struct typedef
 *
 *  GPLv2 only - Copyright (C) 2008
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
 * @file   eurephia_values_struct.h
 * @author David Sommerseth <dazo@users.sourceforge.net>
 * @date   2008-11-05
 *
 * @brief  Definition of the eurephiaVALUES struct
 *
 */

#ifndef   	EUREPHIA_VALUES_STRUCT_H_
# define   	EUREPHIA_VALUES_STRUCT_H_

#include <log.h>

/**
 * eurephiaVALUES is a pointer chain with key/value pairs.  If having several
 * such pointer chains, they can be given different group IDs to separate them,
 * which is especially useful during debugging.
 *
 */
typedef struct __eurephiaVALUES {
	LogContext *log;        /**< Pointer to an established log context, used for logging */
        unsigned int evgid;	/**< Group ID, all elements in the same chain should have the same value */
        unsigned int evid;	/**< Unique ID per element in a pointer chain */
        char *key;		/**< The key name of a value */
        char *val;		/**< The value itself */
        struct __eurephiaVALUES *next; /**< Pointer to the next element in the chain. NULL == end of chain */
} eurephiaVALUES;

#endif 	    /* !EUREPHIA_VALUES_STRUCT_H_ */
