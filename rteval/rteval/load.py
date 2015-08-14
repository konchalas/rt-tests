#
#   Copyright 2009,2010   Clark Williams <williams@redhat.com>
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
import os
import os.path
import time
import subprocess
import threading

class Load(threading.Thread):
    def __init__(self, name="<unnamed>", params={}):
        threading.Thread.__init__(self)
        self.name = name
        self.builddir = params.setdefault('builddir', os.path.abspath("../build"))	# abs path to top dir
        self.srcdir = params.setdefault('srcdir', os.path.abspath("../loadsource"))	# abs path to src dir
        self.num_cpus = params.setdefault('numcores', 1)
        self.debugging = params.setdefault('debugging', False)
        self.source = params.setdefault('source', None)
        self.reportdir = params.setdefault('reportdir', os.getcwd())
        self.logging = params.setdefault('logging', False)
        self.memsize = params.setdefault('memsize', (0, 'GB'))
        self.params = params
        self.ready = False
        self.mydir = None
        self.startevent = threading.Event()
        self.stopevent = threading.Event()

        if not os.path.exists(self.builddir):
            os.makedirs(self.builddir)

    def debug(self, str):
        if self.debugging: print "%s: %s" % (self.name, str)

    def isReady(self):
        return self.ready

    def shouldStop(self):
        return self.stopevent.isSet()

    def shouldStart(self):
        return self.startevent.isSet()

    def setup(self, builddir, tarball):
        pass

    def build(self, builddir):
        pass

    def runload(self, rundir):
        pass

    def run(self):
        if self.shouldStop():
            return
        self.setup()
        if self.shouldStop():
            return
        self.build()
        while True:
            if self.shouldStop():
                return
            self.startevent.wait(1.0)
            if self.shouldStart():
                break
        self.runload()

    def report(self):
        pass

    def genxml(self, x):
        pass

    def open_logfile(self, name):
        return os.open(os.path.join(self.reportdir, "logs", name), os.O_CREAT|os.O_WRONLY)
