/* eurephia_xml.h  --  Generic helper functions for XML parsing
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
 * @file   eurephia_xml.h
 * @author David Sommerseth <dazo@users.sourceforge.net>
 * @date   2008-12-15
 *
 * @brief  Generic XML parser functions
 *
 */


#ifndef   	EUREPHIA_XML_H_
#define   	EUREPHIA_XML_H_

#include <stdarg.h>

#include <libxml/tree.h>

/**
 * Simple iterator macro for iterating xmlNode pointers
 *
 * @param start  Pointer to an xmlNode where to start iterating
 * @param itn    An xmlNode pointer which will be used for the iteration.
 */
#define foreach_xmlnode(start, itn)  for( itn = start; itn != NULL; itn = itn->next )

char *xmlGetAttrValue(xmlAttr *properties, const char *key);
xmlNode *xmlFindNode(xmlNode *node, const char *key);

inline char *xmlExtractContent(xmlNode *n);
inline char *xmlGetNodeContent(xmlNode *node, const char *key);
char *xmlNodeToString(LogContext *log, xmlNode *node);

#endif 	    /* !EUREPHIA_XML_H_ */
