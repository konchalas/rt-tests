#
#   testclient.py
#   XML-RPC test client for testing the supported XML-RPC API 
#   in the rteval server.
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

import sys
import libxml2
import StringIO

sys.path.append('../rteval')
import rtevalclient

print "** Creating doc"
d = libxml2.newDoc("1.0")
n = libxml2.newNode('TestNode1')
d.setRootElement(n)
n2 = n.newTextChild(None, 'TestNode2','Just a little test')
n2.newProp('test','true')

for i in range(1,5):
    n2 = n.newTextChild(None, 'TestNode3', 'Test line %i' %i)

print "** Doc to be sent"
d.saveFormatFileEnc('-','UTF-8', 1)


print "** Testing API"
client = rtevalclient.rtevalclient("http://localhost:65432/rteval/API1/")

print "** 1: Hello(): %s" % str(client.Hello())
status = client.SendReport(d)
print "** 2: SendReport(xmlDoc): %s" % str(status)

