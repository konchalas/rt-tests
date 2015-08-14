/*  configparser.h - Read and parse config files
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
 * @file   configparser.h
 * @author David Sommerseth <davids@redhat.com>
 * @date   2009-10-01
 *
 * @brief  Config file parser
 *
 */

#ifndef _CONFIGPARSER_H
#define _CONFIGPARSER_H

eurephiaVALUES *read_config(LogContext *log, eurephiaVALUES *prgargs, const char *section);

#endif
