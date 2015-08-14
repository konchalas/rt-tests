#
#   rteval_xmlrpc.wsgi
#   XML-RPC handler for the rteval server, using mod_wsgi
#
#   Copyright 2011      David Sommerseth <davids@redhat.com>
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

from wsgiref.simple_server import make_server
import types
from xmlrpclib import dumps, loads, Fault
from xmlrpc_API1 import XMLRPC_API1
from rteval.rtevalConfig import rtevalConfig

def rtevalXMLRPC_Dispatch(method, args):
    # Default configuration
    defcfg = {'xmlrpc_server': { 'datadir':     './var/lib/rteval',
                                 'db_server':   'localhost',
                                 'db_port':     5432,
                                 'database':    'rteval',
                                 'db_username': 'rtevxmlrpc',
                                 'db_password': 'rtevaldb'
                                 }
              }

    # Fetch configuration
    cfg = rtevalConfig(defcfg)
    cfg.Load(append=True)

    # Prepare an object for executing the query
    xmlrpc = XMLRPC_API1(config=cfg.GetSection('xmlrpc_server'))

    # Exectute it
    result = xmlrpc.Dispatch(method, args)

    # Send the result
    if type(result) == types.TupleType:
        return dumps(result, None, methodresponse=1)
    else:
        return dumps((result,), None, methodresponse=1)


def rtevalXMLRPC_handler(environ, start_response):

   # the environment variable CONTENT_LENGTH may be empty or missing
   try:
      request_body_size = int(environ.get('CONTENT_LENGTH', 0))
   except (ValueError):
      request_body_size = 0

   # When the method is POST the query string will be sent
   # in the HTTP request body which is passed by the WSGI server
   # in the file like wsgi.input environment variable.
   try:
       if (environ['REQUEST_METHOD'] != 'POST') or (request_body_size < 1):
           raise Exception('Error in request')

       request_body = environ['wsgi.input'].read(request_body_size)
       try:
           args, method = loads(request_body)
       except:
           raise Exception('Invalid XML-RPC request')

       # Execute the XML-RPC call
       status = '200 OK'
       cont_type = 'text/xml'
       response = [rtevalXMLRPC_Dispatch(method, args)]
   except Exception, ex:
       status = '500 Internal server error: %s' % str(ex)
       cont_type = 'text/plain'
       response = [
           '500 Internal server error\n',
           'ERROR: %s' % str(ex)
           ]
       import traceback, sys
       traceback.print_exc(file=sys.stderr)

   response_headers = [('Content-Type', cont_type),
                       ('Content-Length', str(len("".join(response))))]
   start_response(status, response_headers)
   return response


if __name__ == '__main__':
    #
    # Simple stand-alone XML-RPC server, if started manually
    # Not suitable for production environments, but for testing
    #
    httpd = make_server('localhost', 65432, rtevalXMLRPC_handler)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print "\nShutting down"

