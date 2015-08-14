#
#   database.py
#   Library for processing results from XMLSQLparser and
#   query a PostgreSQL database based on the input data
#
#   Copyright 2009      David Sommerseth <davids@redhat.com>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
#
#   For the avoidance of doubt the "preferred form" of this code is one which
#   is in an open unpatent encumbered format. Where cryptographic key signing
#   forms part of the process of creating an executable the information
#   including keys needed to generate an equivalently functional executable
#   are deemed to be part of the source code.
#

import psycopg2
import types

class Database(object):
    def __init__(self, host=None, port=None, user=None, password=None, database=None,
                 noaction=False, debug=False):
        self.noaction = noaction
        self.debug = debug

        dsnd = {}
        if host is not None:
            dsnd['host'] = host
            dsnd['sslmode'] = 'require'
        if port is not None:
            dsnd['port'] = str(port)
            dsnd['sslmode'] = 'require'
        if user is not None:
            dsnd['user'] = user
        if password is not None:
            dsnd['password'] = password
        if database is not None:
            dsnd['dbname'] = database

        dsn = " ".join(["%s='%s'" %(k,v) for (k,v) in dsnd.items()])
        self.conn = not self.noaction and psycopg2.connect(dsn) or None


    def INSERT(self, sqlvars):
        #
        # Validate input data
        #
        if type(sqlvars) is not types.DictType:
            raise AttributeError,'Input parameter is not a Python dict'

        try:
            sqlvars['table']
            sqlvars['fields']
            sqlvars['records']
        except KeyError, err:
            raise KeyError, "Input dictionary do not contain a required element: %s", str(err)

        if type(sqlvars['fields']) is not types.ListType:
            raise AttributeError,"The 'fields' element is not a list of fields"

        if type(sqlvars['records']) is not types.ListType:
            raise AttributeError,"The 'records' element is not a list of fields"

        if len(sqlvars['records']) == 0:
            return True

        try:
            sqlvars['returning']
        except:
            sqlvars['returning'] = None

        #
        # Build SQL template
        #
        sqlstub = "INSERT INTO %s (%s) VALUES (%s)" % (
            sqlvars['table'],
            ",".join(sqlvars['fields']),
            ",".join(["%%(%s)s" % f for f in sqlvars['fields']])
            )

        # Get a database cursor
        curs = not self.noaction and self.conn.cursor() or None

        #
        # Loop through all records and insert them into the database
        #
        results = []
        for rec in sqlvars['records']:
            if type(rec) is not types.ListType:
                raise AttributeError, "The field values inside the 'records' list must be in a list"

            # Create a dictionary, which will be used for the SQL operation
            values = {}
            for i in range(0, len(sqlvars['fields'])):
                values[sqlvars['fields'][i]] = rec[i]

            if self.debug:
                print "SQL QUERY: ==> %s" % (sqlstub % values)

            # Do the INSERT query
            if not self.noaction:
                curs.execute(sqlstub, values)

            # If a return value for the INSERT is defined, catch that one
            if not self.noaction and sqlvars['returning']:
                # The psycopg2 do not handle INSERT INTO ... RETURNING column queries, so we can only use
                # this on tables with oid and do the look up that way
                vls = {"table": sqlvars['table'], 'colname': sqlvars['returning'], 'oid': str(curs.lastrowid)}
                curs.execute("SELECT %(colname)s FROM %(table)s WHERE oid='%(oid)s'" % vls)
                results.append(curs.fetchone()[0])
            else:
                results.append(True)

        if not self.noaction:
            curs.close()
        return results


    def DELETE(self, table, where):
        try:
            sql = "DELETE FROM %s WHERE %s" % (
                table,
                " AND ".join(["%s = %%(%s)s" % (k,k) for (k,v) in where.items()])
                )

            if self.debug:
                print "SQL QUERY ==> %s" % (sql % where)

            if not self.noaction:
                curs = self.conn.cursor()
                curs.execute(sql, where)
                delrows = curs.rowcount
                curs.close()
                return delrows
            else:
                return 0
        except Exception, err:
            raise Exception, "** SQL ERROR ** %s\n** SQL ERROR ** Message: %s" % ((sql % where), str(err))

    def SELECT(self, table, fields, joins=None, where=None):
        curs = not self.noaction and self.conn.cursor() or None

        # Query
        try:
            sql = "SELECT %s FROM %s %s %s" % (
                ",".join(fields),
                table,
                joins and "%s" % joins or "",
                where and "WHERE %s" % " AND ".join(["%s = %%(%s)s" % (k,k) for (k,v) in where.items()] or "")
                )
            if self.debug:
                print "SQL QUERY: ==> %s" % (sql % where)
            if not self.noaction:
                curs.execute(sql, where)
            else:
                # If no action is setup (mainly for debugging), return empty result set
                return {"table": table, "fields": [], "records": []}
        except Exception, err:
            raise Exception, "** SQL ERROR *** %s\n** SQL ERROR ** Message: %s" % (where and (sql % where) or sql, str(err))

        # Extract field names
        fields = []
        for fn in curs.description:
            fields.append(fn[0])

        # Extract records
        records = []
        for dbrec in curs.fetchall():
            values = []
            for val in dbrec:
                values.append(val)
            records.append(values)

        curs.close()
        if self.debug:
            print "database::SELECT() result ** Fields: %s\nRecords: %s" % (fields, records)
        return {"table": table, "fields": fields, "records": records}

    def COMMIT(self):
        # Commit the work
        if not self.noaction:
            self.conn.commit()

    def ROLLBACK(self):
        # Abort / rollback the current work
        if not self.noaction:
            self.conn.rollback()


    def GetValue(self, dbres, recidx, field):
        "Helper function to easy extract a field from a record set"

        # Check that input data good
        if type(dbres) is not types.DictType:
            raise AttributeError,'Database result parameter is not a Python dict'

        try:
            dbres['table']
            dbres['fields']
            dbres['records']
        except KeyError, err:
            raise KeyError, "Database result parameter do not contain a required element: %s", str(err)

        if type(dbres['fields']) is not types.ListType:
            raise AttributeError,"The 'fields' element is not a list of fields"

        if type(dbres['records']) is not types.ListType:
            raise AttributeError,"The 'records' element is not a list of fields"

        # Return None when we're going out of boundaries
        if recidx >= len(dbres['records']):
            return None

        if type(field) == types.StringType:
            # Find the field index of the field name in the records set
            try:
                fidx = dbres['fields'].index(field)
            except ValueError:
                raise Exception, "Field '%s' is not found in the database result" % field
        elif type(field) == types.IntType:
            # If the field value is integer, assume it is the numeric field id
            if field >= len(dbres['fields']):
                raise Exception, "Field id '%i' is too high.  No field available" % field
            fidx = field

        # Return the value
        return dbres['records'][recidx][fidx]


    def NumTuples(self, dbres):
        # Check that input data good
        if type(dbres) is not types.DictType:
            raise AttributeError,'Database result parameter is not a Python dict'

        try:
            dbres['table']
            dbres['fields']
            dbres['records']
        except KeyError, err:
            raise KeyError, "Database result parameter do not contain a required element: %s", str(err)

        if type(dbres['records']) is not types.ListType:
            raise AttributeError,"The 'records' element is not a list of fields"

        return len(dbres['records'])
