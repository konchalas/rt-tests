#  
#   hackbench.py - class to manage an instance of hackbench load
#
#   Copyright 2009,2010   Clark Williams <williams@redhat.com>
#   Copyright 2009,2010   David Sommerseth <davids@redhat.com>
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
import time
import glob
import subprocess
import errno
from signal import SIGTERM
from signal import SIGKILL
sys.pathconf = "."
import load

class Hackbench(load.Load):
    def __init__(self, params={}):
        load.Load.__init__(self, "hackbench", params)

    def __del__(self):
        null = open("/dev/null", "w")
        subprocess.call(['killall', '-9', 'hackbench'], 
                        stdout=null, stderr=null)
        os.close(null)

    def setup(self):
        'calculate arguments based on input parameters'
        (mem, units) = self.memsize
        if units == 'KB':
            mem = mem / (1024.0 * 1024.0)
        elif units == 'MB':
            mem = mem / 1024.0
        elif units == 'TB':
            mem = mem * 1024
        ratio = float(mem) / float(self.num_cpus)
        if ratio >= 0.20:
            mult = float(self.params.setdefault('jobspercore', 2))
        else:
            print "hackbench: low memory system (%f GB/core)! Not running\n" % ratio
            mult = 0
        self.jobs = self.num_cpus * mult

        self.args = ['hackbench',  '-P',
                     '-g', str(self.jobs), 
                     '-l', str(self.params.setdefault('loops', '100')),
                     '-s', str(self.params.setdefault('datasize', '100'))
                     ]
        self.err_sleep = 5.0

    def build(self):
        self.ready = True

    def start_hackbench(self, inf, outf, errf):
        self.debug("running: %s" % " ".join(self.args))
        return subprocess.Popen(self.args, stdin=inf, stdout=outf, stderr=errf)

    def runload(self):
        # if we don't have any jobs just wait for the stop event and return
        if self.jobs == 0:
            self.stopevent.wait()
            return
        null = os.open("/dev/null", os.O_RDWR)
        if self.logging:
            out = self.open_logfile("hackbench.stdout")
            err = self.open_logfile("hackbench.stderr")
        else:
            out = err = null
        self.debug("starting loop (jobs: %d)" % self.jobs)

        p = self.start_hackbench(null, out, err)
        while not self.stopevent.isSet():
            try:
                # if poll() returns an exit status, restart
                if p.poll() != None:
                    p = self.start_hackbench(null, out, err)
                time.sleep(1.0)
            except OSError, e:
                if e.errno != errno.ENOMEM:
                    raise
                # Catch out-of-memory errors and wait a bit to (hopefully) 
                # ease memory pressure
                print "hackbench: %s, sleeping for %f seconds" % (e.strerror, self.err_sleep)
                time.sleep(self.err_sleep)
                if self.err_sleep < 60.0:
                    self.err_sleep *= 2.0
                if self.err_sleep > 60.0:
                    self.err_sleep = 60.0

        self.debug("stopping")
        if p.poll() == None:
            os.kill(p.pid, SIGKILL)
        p.wait()
        self.debug("returning from runload()")
        os.close(null)
        if self.logging:
            os.close(out)
            os.close(err)

    def genxml(self, x):
        x.taggedvalue('command_line', self.jobs and ' '.join(self.args) or None, 
                      {'name':'hackbench', 'run': self.jobs and '1' or '0'})

def create(params = {}):
    return Hackbench(params)


if __name__ == '__main__':
    h = Hackbench(params={'debugging':True, 'verbose':True})
    h.run()
