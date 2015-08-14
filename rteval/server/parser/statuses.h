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
 * @file   statuses.h
 * @author David Sommerseth <davids@redhat.com>
 * @date   Wed Oct 21 11:17:24 2009
 *
 * @brief  Status values used by rteval-parserd
 *
 */

#ifndef _RTEVAL_STATUS_H
#define _RTEVAL_STATUS_H

#define STAT_NEW       0         /**< New, unparsed report in the submission queue */
#define STAT_ASSIGNED  1         /**< Submission is assigned to a parser */
#define STAT_INPROG    2         /**< Parsing has started */
#define STAT_SUCCESS   3         /**< Report parsed successfully */
#define STAT_UNKNFAIL  4         /**< Unkown failure */
#define STAT_XMLFAIL   5         /**< Failed to parse the report XML file */
#define STAT_SYSREG    6         /**< System registration failed */
#define STAT_RTERIDREG 7         /**< Failed to get a new rterid value for the rteval run */
#define STAT_GENDB     8         /**< General database error */
#define STAT_RTEVRUNS  9         /**< Registering rteval run information failed */
#define STAT_CYCLIC    10        /**< Registering cyclictest results failed */
#define STAT_REPMOVE   11        /**< Failed to move the report file */
#define STAT_FTOOBIG   12        /**< Report is too big (see config parameter: max_report_size) */

#endif
