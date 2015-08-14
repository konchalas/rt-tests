/* eurephia_xml.c  --  Generic helper functions for XML parsing
 *
 * This version is modified to work outside the eurephia project.
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
 * @file   eurephia_xml.c
 * @author David Sommerseth <dazo@users.sourceforge.net>
 * @date   2008-12-15
 *
 * @brief  Generic XML parser functions
 *
 *
 */

#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include <libxml/tree.h>
#include <libxml/xmlsave.h>
#include <libxml/xmlstring.h>

#include <eurephia_nullsafe.h>


/**
 * Retrieves a given XML node attribute/property
 *
 * @param attr xmlAttr pointer from an xmlNode pointer.
 * @param key  The attribute name to search for
 *
 * @return The value of the found attribute.  If not found, NULL is returned.
 */
char *xmlGetAttrValue(xmlAttr *attr, const char *key) {
        xmlAttr *aptr;
        xmlChar *x_key = NULL;

        x_key = xmlCharStrdup(key);
        assert( x_key != NULL );

        for( aptr = attr; aptr != NULL; aptr = aptr->next ) {
                if( xmlStrcmp(aptr->name, x_key) == 0 ) {
                        free_nullsafe(x_key);
                        return (char *)(aptr->children != NULL ? aptr->children->content : NULL);
                }
        }
        free_nullsafe(x_key);
        return NULL;
}


/**
 * Loops through a xmlNode chain to look for a given tag.  The search is not recursive.
 *
 * @param node xmlNode pointer where to look
 * @param key  the name of the XML tag to find
 *
 * @return xmlNode pointer to the found xmlNode.  NULL is returned if not found.
 */
xmlNode *xmlFindNode(xmlNode *node, const char *key) {
        xmlNode *nptr = NULL;
        xmlChar *x_key = NULL;

        if( (node == NULL) || (node->children == NULL) ) {
                return NULL;
        }

        x_key = xmlCharStrdup(key);
        assert( x_key != NULL );

        for( nptr = node->children; nptr != NULL; nptr = nptr->next ) {
                if( xmlStrcmp(nptr->name, x_key) == 0 ) {
                        free_nullsafe(x_key);
                        return nptr;
                }
        }
        free_nullsafe(x_key);
        return NULL;
}


/**
 * Return the text content of a given xmlNode
 *
 * @param n xmlNode to extract the value from.
 *
 * @return returns a char pointer with the text contents of an xmlNode.
 */
inline char *xmlExtractContent(xmlNode *n) {
        return (char *) (((n != NULL) && (n->children != NULL)) ? n->children->content : NULL);
}


/**
 * Get the text contents of a given xmlNode
 *
 * @param node An xmlNode pointer where to look for the contents
 * @param key  Name of the tag to retrieve the content of.
 *
 * @return Returns a string with the text content, if the node is found.  Otherwise, NULL is returned.
 */
inline char *xmlGetNodeContent(xmlNode *node, const char *key) {
        return xmlExtractContent(xmlFindNode(node, key));
}


/**
 * Serialises an xmlNode to a string
 *
 * @param log   Log context
 * @param node Input XML node to be serialised
 *
 * @return Returns a pointer to a new buffer containing the serialised data.  This buffer must be freed
 *         after usage
 */
char *xmlNodeToString(LogContext *log, xmlNode *node) {
	xmlBuffer *buf = NULL;
	xmlSaveCtxt *serctx = NULL;
	char *ret = NULL;

	if( node == NULL ) {
		writelog(log, LOG_ALERT, "xmlNodeToString: Input data is NULL");
		return NULL;
	}

	buf = xmlBufferCreate();
	assert( buf != NULL );

	serctx = xmlSaveToBuffer(buf, "UTF-8", XML_SAVE_NO_EMPTY|XML_SAVE_NO_DECL);
	assert( serctx != NULL );

	if( xmlSaveTree(serctx, node) < 0 ) {
		writelog(log, LOG_ALERT, "xmlNodeToString: Failed to serialise xmlNode");
		return NULL;
	}
	xmlSaveClose(serctx);

	ret = strdup_nullsafe((char *) xmlBufferContent(buf));
	xmlBufferFree(buf);
	return ret;
}
