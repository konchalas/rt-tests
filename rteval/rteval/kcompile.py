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
import time
import glob
import subprocess
from signal import SIGTERM
sys.pathconf = "."
import load
import xmlout

kernel_prefix="linux-4.1"

class Kcompile(load.Load):
    def __init__(self, params={}):
        load.Load.__init__(self, "kcompile", params)

    def setup(self):
        # find our source tarball
        if self.params.has_key('tarball'):
            tarfile = os.path.join(self.srcdir, self.params.tarfile)
            if not os.path.exists(tarfile):
                raise RuntimeError, " tarfile %s does not exist!" % tarfile
            self.source = tarfile
        else:
            tarfiles = glob.glob(os.path.join(self.srcdir, "%s*" % kernel_prefix))
            if len(tarfiles):
                self.source = tarfiles[0]
            else:
                raise RuntimeError, " no kernel tarballs found in %s" % self.srcdir

        # check for existing directory
        kdir=None
        names=os.listdir(self.builddir)
        for d in names:
            if d.startswith(kernel_prefix):
                kdir=d
                break
        if kdir == None:
            self.debug("unpacking kernel tarball")
            tarargs = ['tar', '-C', self.builddir, '-x']
            if self.source.endswith(".bz2"):
                tarargs.append("-j")
            elif self.source.endswith(".gz"):
                tarargs.append("-z")
            tarargs.append("-f")
            tarargs.append(self.source)
            try:
                subprocess.call(tarargs)
            except:
                self.debug("untar'ing kernel self.source failed!")
                sys.exit(-1)
            names = os.listdir(self.builddir)
            for d in names:
                self.debug("checking %s" % d)
                if d.startswith(kernel_prefix):
                    kdir=d
                    break
        if kdir == None:
            raise RuntimeError, "Can't find kernel directory!"
        self.mydir = os.path.join(self.builddir, kdir)
        self.debug("mydir = %s" % self.mydir)

    def build(self):
        self.debug("setting up all module config file in %s" % self.mydir)
        null = os.open("/dev/null", os.O_RDWR)
        out = self.open_logfile("kcompile-build.stdout")
        err = self.open_logfile("kcompile-build.stderr")
        # clean up from potential previous run
        try:
            ret = subprocess.call(["make", "-C", self.mydir, "mrproper", "allmodconfig"], 
                                  stdin=null, stdout=out, stderr=err)
            if ret:
                raise RuntimeError, "kcompile setup failed: %d" % ret
        except KeyboardInterrupt, m:
            self.debug("keyboard interrupt, aborting")
            return
        self.debug("ready to run")
        self.ready = True
        os.close(null)
        os.close(out)
        os.close(err)

    def calc_numjobs(self):
        mult = int(self.params.setdefault('jobspercore', 1))
        mem = self.memsize[0]
        if self.memsize[1] == 'KB':
            mem = mem / (1024.0 * 1024.0)
        elif self.memsize[1] == 'MB':
            mem = mem / 1024.0
        elif self.memsize[1] == 'TB':
            mem = mem * 1024
        ratio = float(mem) / float(self.num_cpus)
        if ratio > 1.0:
            njobs = self.num_cpus * mult
        else:
            self.debug("low memory system (%f GB/core)! Dropping jobs to one per core\n" % ratio)
            njobs = self.num_cpus
        return njobs

    def runload(self):
        null = os.open("/dev/null", os.O_RDWR)
        if self.logging:
            out = self.open_logfile("kcompile.stdout")
            err = self.open_logfile("kcompile.stderr")
        else:
            out = err = null

        njobs = self.calc_numjobs()
        self.debug("starting loop (jobs: %d)" % njobs)
        self.args = ["make", "-C", self.mydir, 
                     "-j%d" % njobs ] 
        p = subprocess.Popen(self.args, 
                             stdin=null,stdout=out,stderr=err)
        while not self.stopevent.isSet():
            time.sleep(1.0)
            if p.poll() != None:
                r = p.wait()
                self.debug("restarting compile job (exit status: %s)" % r)
                p = subprocess.Popen(self.args,
                                     stdin=null,stdout=out,stderr=err)
        self.debug("out of stopevent loop")
        if p.poll() == None:
            self.debug("killing compile job with SIGTERM")
            os.kill(p.pid, SIGTERM)
        p.wait()
        os.close(null)
        if self.logging:
            os.close(out)
            os.close(err)

    def genxml(self, x):
        x.taggedvalue('command_line', ' '.join(self.args), {'name':'kcompile', 'run':'1'})

def create(params = {}):
    return Kcompile(params)
    
