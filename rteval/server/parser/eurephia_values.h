/* eurephia_values.h  --  Generic interface for processing key->value pairs
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
 *
 */

/**
 * @file   eurephia_values.h
 * @author David Sommerseth <dazo@users.sourceforge.net>
 * @date   2008-08-06
 *
 * @brief  Generic interface for handling key->value pairs
 *
 */

#include <eurephia_values_struct.h>

#ifndef         EUREPHIA_VALUES_H_
#define         EUREPHIA_VALUES_H_


eurephiaVALUES *eGet_valuestruct(eurephiaVALUES *vls, const char *key);
char *eGet_value(eurephiaVALUES *vls, const char *key);

eurephiaVALUES *eCreate_value_space(LogContext *log, int evid);

void eAdd_valuestruct(eurephiaVALUES *vls, eurephiaVALUES *newval);
void eAdd_value(eurephiaVALUES *vls, const char *key, const char *val);
void eUpdate_value(eurephiaVALUES *vls, const char *key, const char *newval, const int addunkn);
eurephiaVALUES *eUpdate_valuestruct(eurephiaVALUES *vls, eurephiaVALUES *newval, const int addunkn);
eurephiaVALUES *eRemove_value(eurephiaVALUES *vls, unsigned int evgid, unsigned int evid);
unsigned int eCount(eurephiaVALUES *vls);

/**
 * Front-end function for eFree_values_func().  Frees eurephiaVALUES pointer chain and
 * sets the pointer to NULL.
 *
 * @param v eurephiaVALUES pointer which is being freed.
 *
 */
#define eFree_values(v) { eFree_values_func(v); v = NULL; }
void eFree_values_func(eurephiaVALUES *vls);

#endif      /* !EUREPHIA_VALUES_H_ */
