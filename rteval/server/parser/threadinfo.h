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
 * @file   threadinfo.h
 * @author David Sommerseth <davids@redhat.com>
 * @date   Thu Oct 15 11:47:51 2009
 *
 * @brief  Shared info between the main() and parsethread() functions
 *
 */

#ifndef _THREADINFO_H
#define _THREADINFO_H

#include <mqueue.h>
#include <libxslt/transform.h>

/**
 *  Thread slot information.  Each thread slot is assigned with one threadData_t element.
 */
typedef struct {
        int *shutdown;                /**< If set to 1, the thread should shut down */
        int *threadcount;             /**< Number of active worker threads */
        pthread_mutex_t *mtx_thrcnt;  /**< Mutex lock for updating active worker threads */
        mqd_t msgq;                   /**< POSIX MQ descriptor */
        pthread_mutex_t *mtx_sysreg;  /**< Mutex locking, to avoid clashes with registering systems */
        unsigned int id;              /**< Numeric ID for this thread */
        dbconn *dbc;                  /**< Database connection assigned to this thread */
        xsltStylesheet *xslt;         /**< XSLT stylesheet assigned to this thread */
        const char *destdir;          /**< Directory where to put the parsed reports */
        unsigned int max_report_size; /**< Maximum accepted file size of reports (config: max_report_size) */
} threadData_t;

#endif
