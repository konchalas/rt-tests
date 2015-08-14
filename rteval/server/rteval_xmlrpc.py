#
#   rteval_xmlrpc.py
#   XML-RPC handler for mod_python which will receive requests
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

import types
from mod_python import apache
from xmlrpclib import dumps, loads, Fault
from xmlrpc_API1 import XMLRPC_API1
from rteval.rtevalConfig import rtevalConfig


def Dispatch(req, method, args):
    # Default configuration
    defcfg = {'xmlrpc_server': { 'datadir':     '/var/lib/rteval',
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
        req.write(dumps(result, None, methodresponse=1))
    else:
        req.write(dumps((result,), None, methodresponse=1))


def handler(req):
    # Only accept POST requests
    if req.method != 'POST':
        req.content_type = 'text/plain'
        req.send_http_header()
        req.write("Not valid XML-RPC POST request")
        return apache.OK

    # Fetch the request
    body = req.read()

    # Prepare response
    req.content_type = "text/xml"
    req.send_http_header()

    # Process request
    try:
        args, method = loads(body)
    except:
        fault = Fault(0x001, "Invalid XML-RPC error")
        req.write(dumps(fault, methodresponse=1))
        return apache.OK

    # Execute it.  The calling function is
    # responsive for responding to the request.
    Dispatch(req, method, args)

    return apache.OK
