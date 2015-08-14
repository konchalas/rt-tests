#
#   xmlrpc_API1.py
#   XML-RPC functions supported by the API1 version for the rteval server
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
import os
import bz2
import base64
import libxml2
import string
import platform
import rtevaldb


class XMLRPC_API1():
    def __init__(self, config=None, debug=False, nodbaction=False):
        # Some defaults
        self.apiversion = 1
        self.fnametrans = string.maketrans("/\\.", "::_") # replace path delimiters in filenames
        self.debug = debug
        self.nodbaction = nodbaction
        self.config = config


    def __mkdatadir(self, dirpath):
        startdir = os.getcwd()
        if dirpath[0] == '/':
            os.chdir('/')
        for dir in dirpath.split("/"):
            if dir is '':
                continue
            if not os.path.exists(dir):
                os.mkdir(dir, 0700)
            os.chdir(dir)
        os.chdir(startdir)


    def __getfilename(self, dir, fname, ext, comp):
        idx = 0
        if comp:
            filename = "%s/%s/%s%s.bz2" % (self.config.datadir, dir, fname.translate(self.fnametrans), ext)
        else:
            filename = "%s/%s/%s%s" % (self.config.datadir, dir, fname.translate(self.fnametrans), ext)

        while 1:
            if not os.path.exists(filename):
                return filename
            idx += 1
            if comp:
                filename = "%s/%s/%s-{%i}.bz2" % (self.config.datadir, dir,
                                                  fname.translate(self.fnametrans), idx)
            else:
                filename = "%s/%s/%s-{%i}" % (self.config.datadir, dir,
                                              fname.translate(self.fnametrans), idx)


    def Dispatch(self, method, params):
        # Call the method requested
        # FIXME: Improve checking for valid methods
        func = getattr(self, method)
        return func(*params)


    def _dispatch(self, method, params):
        "Wrapper method for Dispatch(), used by xmlrpclib in a local test server. Only used for testing."
        return self.Dispatch(method, params)


    def SendReport(self, clientid, xmlbzb64):
        decompr = bz2.BZ2Decompressor()
        xmldoc = libxml2.parseDoc(decompr.decompress(base64.b64decode(xmlbzb64)))

        # Save a copy of the report on the file system
        # Make sure we have a directory to write files into
        self.__mkdatadir(self.config.datadir + '/queue/')
        fname = self.__getfilename('queue/', ('%s' % clientid), '.xml', False)
        xmldoc.saveFormatFileEnc(fname,'UTF-8',1)
        if self.debug:
            print "Copy of report: %s" % fname

        # Register the submission and put it in a parse queue
        rterid = rtevaldb.register_submission(self.config, clientid, fname,
                                                        debug=self.debug, noaction=self.nodbaction)
        if self.nodbaction:
            rterid = 999999999 # Fake ID when no database registration is done

        return rterid


    def Hello(self, clientid):
        return {"greeting": "Hello %s" % clientid,
                "server": platform.node(),
                "APIversion": self.apiversion}


    def DatabaseStatus(self):
        return rtevaldb.database_status(self.config)
