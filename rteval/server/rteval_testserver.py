#
#   rteval_testserver.py
#   Local XML-RPC test server.  Can be used to verify XML-RPC behavoiur
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
import sys
import signal
from SimpleXMLRPCServer import SimpleXMLRPCServer
from SimpleXMLRPCServer import SimpleXMLRPCRequestHandler
from optparse import OptionParser

import xmlrpc_API1
from Logger import Logger

# Default values
LISTEN="127.0.0.1"
PORT=65432

# Restrict to a particular path.
class RequestHandler(SimpleXMLRPCRequestHandler):
    rpc_paths = ('/rteval/API1/',)


class RTevald_config(object):
    def __init__(self):
        self.config = {'datadir': '/tmp/rteval-xmlrpc-testsrv',
                       'db_server': 'localhost',
                       'db_port': 5432,
                       'database': 'dummy',
                       'db_username': None,
                       'db_password': None}
        self.__update_vars()

    def __update_vars(self):
        for k in self.config.keys():
            self.__dict__[k] = self.config[k]


class RTevald():
    def __init__(self, options, log):
        self.options = options
        self.log = log
        self.server = None
        self.config = RTevald_config()

    def __prepare_datadir(self):
        startdir = os.getcwd()
        for dir in self.config.datadir.split("/"):
            if dir is '':
                continue
            if not os.path.exists(dir):
                os.mkdir(dir, 0700)
            os.chdir(dir)
        if not os.path.exists('queue'):
            os.mkdir('queue', 0700)
        os.chdir(startdir)

    def StartServer(self):
        # Create server
        self.server = SimpleXMLRPCServer((self.options.listen, self.options.port),
                                         requestHandler=RequestHandler)
        self.server.register_introspection_functions()

        # setup a class to handle requests
        self.server.register_instance(xmlrpc_API1.XMLRPC_API1(self.config, nodbaction=True, debug=True))

        # Run the server's main loop
        self.log.Log("StartServer", "Listening on %s:%i" % (self.options.listen, self.options.port))
        try:
            self.__prepare_datadir()
            self.server.serve_forever()
        except KeyboardInterrupt:
            self.log.Log("StartServer", "Server caught SIGINT")
            self.server.shutdown()
        finally:
            self.log.Log("StartServer", "Server stopped")

    def StopServer(self):
        self.server.shutdown()


logger = None
rtevalserver = None

#
#  M A I N   F U N C T I O N
#

if __name__ == '__main__':
    parser = OptionParser(version="%prog v0.1")

    parser.add_option("-L", "--listen", action="store", dest="listen", default=LISTEN,
                      help="Which interface to listen to [default: %default]", metavar="IPADDR")
    parser.add_option("-P", "--port", action="store", type="int", dest="port", default=PORT,
                      help="Which port to listen to [default: %default]",  metavar="PORT")
    parser.add_option("-l", "--log", action="store", dest="logfile", default=None,
                      help="Where to log requests.", metavar="FILE")

    (options, args) = parser.parse_args()

    logger = Logger(options.logfile, "RTeval")
    rtevalserver = RTevald(options, logger)
    rtevalserver.StartServer()
