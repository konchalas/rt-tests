/* eurephia_values.c  --  Generic interface for processing key->value pairs
 *
 * This version is modified to work outside the eurephia project.
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
 */

/**
 * @file   eurephia_values.c
 * @author David Sommerseth <dazo@users.sourceforge.net>
 * @date   2008-08-06
 *
 * @brief  Generic interface for handling key->value pairs
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <eurephia_nullsafe.h>
#include <eurephia_values_struct.h>



/**
 * Function for freeing up an eurephiaVALUES stack.  This function is normally not called
 * directly, but usually via the eFree_values(...) macro.
 *
 * @param vls  Pointer to a eurephiaVALUES stack to be freed.
 */
void eFree_values_func(eurephiaVALUES *vls) {
	eurephiaVALUES *ptr = NULL, *ptr_next = NULL;

	ptr = vls;
	while( ptr ) {
		free_nullsafe(ptr->key);
		free_nullsafe(ptr->val);

		ptr_next = ptr->next;
		free_nullsafe(ptr);
		ptr = ptr_next;
	}
}


/**
 * Retrieve an eurephiaVALUES element for a given value key
 *
 * @param vls  Pointer to the eurephiaVALUES stack where to search for the element
 * @param key  String containing the key name of the value requested.
 *
 * @return Returns an eurephiaVALUES element on success, otherwise NULL.
 */
eurephiaVALUES *eGet_valuestruct(eurephiaVALUES *vls, const char *key)
{
        eurephiaVALUES *ptr = NULL;

        if( (vls == NULL) || (key == NULL) ) {
                return NULL;
        }

        ptr = vls;
        while( ptr != NULL ) {
                if( (ptr->key != NULL) && (strcmp(key, ptr->key) == 0) ) {
                        return ptr;
                }
                ptr = ptr->next;
        }
        return NULL;
}


/**
 * Retrieves the value of a given key from an eurephiaVALUES stack.
 *
 * @param vls  Pointer to an eurephiaVALUES stack where to search for the value
 * @param key  String containing the key name of the value requested
 *
 * @return Returns a string (char *) with the requested value if found, otherwise NULL.
 */
char *eGet_value(eurephiaVALUES *vls, const char *key)
{
        eurephiaVALUES *ptr = NULL;

        ptr = eGet_valuestruct(vls, key);
        return (ptr != NULL ? ptr->val : NULL);
}


/**
 * Creates a new eurephiaVALUES stack
 *
 * @param log   Log context
 * @param evgid  int value, giving the stack an ID number.  Useful when looking through log files later on.
 *
 * @return Returns an empty eurephiaVALUES struct on success, otherwise NULL.
 */
eurephiaVALUES *eCreate_value_space(LogContext *log, int evgid)
{
        eurephiaVALUES *ptr = NULL;

        ptr = (eurephiaVALUES *) malloc_nullsafe(log, sizeof(eurephiaVALUES) + 2);
	ptr->log = log;
        ptr->evgid = evgid;
        return ptr;
}


/**
 * Adds a new eurephiaVALUES stack to another eurephiaVALUES stack.  If the evgid value differs, it will
 * be overwritten with the value of the destination stack.
 *
 * @param vls    Destination eurephiaVALUES stack
 * @param newval Source eurephiaVALUES stack
 */
void eAdd_valuestruct(eurephiaVALUES *vls, eurephiaVALUES *newval) {
        eurephiaVALUES *ptr = NULL;
        int vid = 0;

        assert(vls != NULL);

        if( (vls->key == NULL) && (vls->val == NULL) && (vls->next == NULL) && (vls->evid == 0)) {
                // Update header record if it is empty, by copying newval record.  Free newval afterwards
                vls->key  = strdup(newval->key);
                vls->val  = strdup(newval->val);
                vls->evid = 0;
                vls->next = NULL;
                eFree_values_func(newval);
        } else {
                // Add values to the value chain, loop to the end and append it
                ptr = vls;
                while( ptr->next != NULL ) {
                        ptr = ptr->next;
                        vid = (vid > ptr->evid ? vid : ptr->evid);
                }
                newval->evid = vid+1;     // Increase the value counter
                newval->evgid = ptr->evgid;
                ptr->next = newval;
        }
}


/**
 * Adds a new key/value pair to an eurephiaVALUES stack
 *
 * @param vls  Destination eurephiaVALUES stack
 * @param key  Key name for the value being stored
 * @param val  Value to be stored
 */
void eAdd_value(eurephiaVALUES *vls, const char *key, const char *val)
{
        eurephiaVALUES *ptr = NULL;

        assert(vls != NULL);

        // Allocate buffer and save values
        ptr = eCreate_value_space(vls->log, vls->evid);
        if( ptr == NULL ) {
		writelog(vls->log, LOG_EMERG, "Failed to add value to the value chain");
		exit(9);
        }
        ptr->key = strdup_nullsafe(key);
        ptr->val = strdup_nullsafe(val);
        ptr->evgid = vls->evgid;

        // Add value struct to the chain
        eAdd_valuestruct(vls, ptr);
}


/**
 * Updates the value of a key in a values stack
 *
 * @param vls      eurephiaVALUES key/value stack to update
 * @param key      String with key name to update
 * @param newval   String with the new value
 * @param addunkn  Add unknown keys.  If set to 1, if the key is not found it will add a new key
 */
void eUpdate_value(eurephiaVALUES *vls, const char *key, const char *newval, const int addunkn) {
	eurephiaVALUES *ptr = NULL;

	assert( (vls != NULL) && (key != NULL) );

	ptr = eGet_valuestruct(vls, key);
	if( ptr ) {
		free_nullsafe(ptr->val);
		ptr->val = strdup_nullsafe(newval);
	} else if( addunkn == 1 ) {
		eAdd_value(vls, key, newval);
	}
}


/**
 * Updates a value struct element based on another value struct element contents (key/value)
 *
 * @param vls      eurephiaVALUES key/value stack to update
 * @param newval   eurephiaVALUES element with the new value
 * @param addunkn  Add unknown keys.  If set to 1, if the key is not found it will add a new key
 *
 * @return Returns a pointer to the first element in the chain.  If the element being updated
 *         was the first element in the old chain, the first element will be a new element with a
 *         new address.
 */
eurephiaVALUES *eUpdate_valuestruct(eurephiaVALUES *vls, eurephiaVALUES *newval, const int addunkn) {
	eurephiaVALUES *ptr = NULL, *prevptr = NULL;

	assert( (vls != NULL) && (newval != NULL) && (newval->key != NULL) );

	prevptr = vls;
	for( ptr = vls; ptr != NULL; ptr = ptr->next ) {
                if( (ptr->key != NULL) && (strcmp(newval->key, ptr->key) == 0) ) {
			newval->evgid = ptr->evgid;
			newval->evid = ptr->evid;
			newval->next = ptr->next;
			ptr->next = NULL;
			if( ptr == vls ) {
				// If the element found is the first one, do special treatment
				eFree_values_func(ptr);
				return newval;
			} else {
				prevptr->next = newval;
				eFree_values_func(ptr);
				return vls;
			}
                }
		prevptr = ptr;
	}

	if( addunkn == 1 ) {
		eAdd_valuestruct(vls, newval);
	}
	return vls;
}


/**
 * Removes the key/value pair identified by evgid and evid from the given eurephiaVALUES chain
 *
 * @param vls    Pointer to an eurephiaVALUES chain with the data
 * @param evgid  Group ID of the chain
 * @param evid   Element ID of the chain element to be removed
 *
 * @return Returns a pointer to the chain.  The pointer is only changed if the first element in the
 *         chain is deleted
 */
eurephiaVALUES *eRemove_value(eurephiaVALUES *vls, unsigned int evgid, unsigned int evid) {
        eurephiaVALUES *ptr = NULL, *prev_ptr = NULL;
        int found = 0;

        // Find the value element
        for( ptr = vls; ptr != NULL; ptr = ptr->next ) {
                if( (ptr->evgid == evgid) && (ptr->evid == evid) ) {
                        found = 1;
                        break;
                }
                prev_ptr = ptr;
        }

        if( !found ) {
                return vls;
        }

        if( ptr != vls ) {
                prev_ptr->next = ptr->next;
                ptr->next = NULL;
                eFree_values_func(ptr);
                return vls;
        } else {
                prev_ptr = ptr->next;
                ptr->next = NULL;
                eFree_values_func(ptr);
                return prev_ptr;
        }
}


/**
 * Counts number of elements in an eurephiaVALUES chain.
 *
 * @param vls eurephiaVALUES pointer to be counted
 *
 * @return Returns number of elements found.
 */
unsigned int eCount(eurephiaVALUES *vls) {
	eurephiaVALUES *ptr = NULL;
	unsigned int c = 0;

	if( vls == NULL ) {
		return 0;
	}
	for(ptr = vls; ptr != NULL; ptr = ptr->next ) {
		c++;
	}
	return c;
}
