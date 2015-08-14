#
#   rtevalclient.py
#   XML-RPC client for sending data to a central rteval result server
#
#   Copyright 2009,2010      David Sommerseth <davids@redhat.com>
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

import xmlrpclib
import libxml2
import StringIO
import bz2
import base64
import platform

class rtevalclient:
    """
    rtevalclient is a library for sending rteval reports to an rteval server via XML-RPC.
    """
    def __init__(self, url="http://rtserver.farm.hsv.redhat.com/rteval/API1/", hostn = None):
        self.srv = xmlrpclib.ServerProxy(url)
        if hostn is None:
            self.hostname = platform.node()
        else:
            self.hostname = hostn

    def Hello(self):
        return self.srv.Hello(self.hostname)

    def DatabaseStatus(self):
        return self.srv.DatabaseStatus()

    def SendReport(self, xmldoc):
        if xmldoc.type != 'document_xml':
            raise Exception, "Input is not XML document"

        fbuf = StringIO.StringIO()
        xmlbuf = libxml2.createOutputBuffer(fbuf, 'UTF-8')
        doclen = xmldoc.saveFileTo(xmlbuf, 'UTF-8')

        compr = bz2.BZ2Compressor(9)
        cmpr = compr.compress(fbuf.getvalue())
        data = base64.b64encode(cmpr + compr.flush())
        ret = self.srv.SendReport(self.hostname, data)
        print "rtevalclient::SendReport() - Sent %i bytes (XML document length: %i bytes, compression ratio: %.02f%%)" % (len(data), doclen, (1-(float(len(data)) / float(doclen)))*100 )
        return ret

    def SendDataAsFile(self, fname, data, decompr = False):
        compr = bz2.BZ2Compressor(9)
        cmprdata = compr.compress(data)
        b64data = base64.b64encode(cmprdata + compr.flush())
        return self.srv.StoreRawFile(self.hostname, fname, b64data, decompr)


    def SendFile(self, fname, decompr = False):
        f = open(fname, "r")
        srvname = self.SendDataAsFile(fname, f.read(), decompr)
        f.close()
        return srvname

