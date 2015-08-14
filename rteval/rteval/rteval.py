#!/usr/bin/python -tt
#
#   rteval - script for evaluating platform suitability for RT Linux
#
#           This program is used to determine the suitability of
#           a system for use in a Real Time Linux environment.
#           It starts up various system loads and measures event
#           latency while the loads are running. A report is generated
#           to show the latencies encountered during the run.
#
#   Copyright 2009,2010,2011,2012   Clark Williams <williams@redhat.com>
#   Copyright 2009,2010,2011,2012   David Sommerseth <davids@redhat.com>
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
import string
import threading
import subprocess
import socket
import optparse
import tempfile
import statvfs
import shutil
import signal
import rtevalclient
import ethtool
import xmlrpclib
import platform
import fnmatch
import glob
from datetime import datetime
from distutils import sysconfig

# put local path at start of list to overide installed methods
sys.path.insert(0, "./rteval")
import util
import load
import cyclictest
import xmlout
import dmi
import rtevalConfig
import rtevalMailer
from cputopology import CPUtopology


pathSave={}
def getcmdpath(which):
    """
    getcmdpath is a method which allows finding an executable in the PATH
    directories to call it from full path
    """
    if not pathSave.has_key(which):
        for path in os.environ['PATH'].split(':'):
            cmdfile = os.path.join(path, which)
            if os.path.isfile(cmdfile) and os.access(cmdfile, os.X_OK):
                pathSave[which] = cmdfile
                break
        if not pathSave[which]:
            raise RuntimeError, "Command '%s' is unknown on this system" % which
    return pathSave[which]


sigint_received = False
def sigint_handler(signum, frame):
    global sigint_received
    sigint_received = True
    print "*** SIGINT received - stopping rteval run ***"

def sigterm_handler(signum, frame):
    raise RuntimeError,  "SIGTERM received!"

class RtEval(object):
    def __init__(self, cmdargs):
        self.version = "1.38"
        self.load_modules = []
        self.workdir = os.getcwd()
        self.reportdir = os.getcwd()
        self.inifile = None
        self.cmd_options = {}
        self.start = datetime.now()
        self.init = 'unknown'

        default_config = {
            'rteval': {
                'verbose'    : False,
                'keepdata'   : True,
                'debugging'  : False,
                'duration'   : '60',
                'sysreport'  : False,
                'reportdir'  : None,
                'reportfile' : None,
                'installdir' : '/usr/share/rteval',
                'srcdir'     : '/usr/share/rteval/loadsource',
                'xmlrpc'     : None,
                'xslt_report': '/usr/share/rteval/rteval_text.xsl',
                'report_interval': '600',
                'logging'    : False,
                },
           'loads' : {
                'kcompile'   : 'module',
                'hackbench'  : 'module',
                },
            'kcompile' : {
                'source'     : 'linux-2.6.39.tar.bz2',
                'jobspercore': '2',
                },
            'hackbench' : {
                'source'     : 'hackbench.tar.bz2',
                'jobspercore': '5',
                },
            'cyclictest' : {
                'interval' : '100',
                'buckets'  : '2000',
                }
            }

        # setup initial configuration
        self.config = rtevalConfig.rtevalConfig(default_config, logfunc=self.info)

        # parse command line options
        self.parse_options(cmdargs)

        # read in config file info
        self.inifile = self.config.Load(self.cmd_options.inifile)

        # copy the command line options into the rteval config section
        # (cmd line overrides config file values)
        self.config.AppendConfig('rteval', self.cmd_options)

        if self.cmd_options.cyclictest_interval != None:
            self.config.AppendConfig('cyclictest', { "interval":self.cmd_options.cyclictest_interval })

        if self.cmd_options.cyclictest_distance != None:
            self.config.AppendConfig('cyclictest', { "distance":self.cmd_options.cyclictest_distance })

        if self.cmd_options.cyclictest_buckets != None:
            self.config.AppendConfig('cyclictest', { "buckets":self.cmd_options.cyclictest_distance })

        if self.cmd_options.cyclictest_priority != None:
            self.config.AppendConfig('cyclictest', { "priority":self.cmd_options.cyclictest_priority })

        if self.cmd_options.hackbench_jobspercore != None:
            self.config.AppendConfig('hackbench', { "jobspercore":self.cmd_options.hackbench_jobspercore })

        if self.cmd_options.kcompile_jobspercore != None:
            self.config.AppendConfig('kcompile', { "jobspercore":self.cmd_options.kcompile_jobspercore })

        self.debug("workdir: %s" % self.workdir)

        # prepare a mailer, if that's configured
        if self.config.HasSection('smtp'):
            self.mailer = rtevalMailer.rtevalMailer(self.config.GetSection('smtp'))
        else:
            self.mailer = None

        self.loads = []
        self.cputopology = None
        self.numcores = None
        self.memsize = None
        self.current_clocksource = None
        self.available_clocksource = None
        self.services = None
        self.kthreads = None
        self.xml = None
        self.baseos = "unknown"
        self.annotate = self.cmd_options.annotate

        if not self.config.xslt_report.startswith(self.config.installdir):
            self.config.xslt_report = os.path.join(self.config.installdir, "rteval_text.xsl")

        if not os.path.exists(self.config.xslt_report):
            raise RuntimeError, "can't find XSL template (%s)!" % self.config.xslt_report

        # Add rteval directory into module search path
        sys.path.insert(0, '%s/rteval' % sysconfig.get_python_lib())

        # generate a set of "junk" characters to use for filtering later
        self.junk = ""
        for c in range(0, 0xff):
            s = chr(c)
            if s not in string.printable:
                self.junk += s
        self.transtable = string.maketrans("", "")

        # If --xmlrpc-submit is given, check that we can access the server
        res = None
        if self.config.xmlrpc:
            self.debug("Checking if XML-RPC server '%s' is reachable" % self.config.xmlrpc)
            attempt = 0
            warning_sent = False
            ping_failed = False
            while attempt < 6:
                try:
                    client = rtevalclient.rtevalclient("http://%s/rteval/API1/" % self.config.xmlrpc)
                    res = client.Hello()
                    attempt = 10
                    ping_failed = False
                except xmlrpclib.ProtocolError:
                    # Server do not support Hello(), but is reachable
                    self.info("Got XML-RPC connection with %s but it did not support Hello()"
                              % self.config.xmlrpc)
                    res = None
                except socket.error, err:
                    self.info("Could not establish XML-RPC contact with %s\n%s"
                              % (self.config.xmlrpc, str(err)))

                    if (self.mailer is not None) and (not warning_sent):
                        self.mailer.SendMessage("[RTEVAL:WARNING] Failed to ping XML-RPC server",
                                                "Server %s did not respond.  Not giving up yet."
                                                % self.config.xmlrpc)
                        warning_sent = True

                    # Do attempts handling
                    attempt += 1
                    if attempt > 5:
                        break # To avoid sleeping before we abort

                    print "Failed pinging XML-RPC server.  Doing another attempt(%i) " % attempt
                    time.sleep(attempt*15) # Incremental sleep - sleep attempts*15 seconds
                    ping_failed = True

            if ping_failed:
                if not self.cmd_options.xmlrpc_noabort:
                    print "ERROR: Could not reach XML-RPC server '%s'.  Aborting." % self.config.xmlrpc
                    sys.exit(2)
                else:
                    print "WARNING: Could not ping the XML-RPC server.  Will continue anyway."

            if res:
                self.info("Verified XML-RPC connection with %s (XML-RPC API version: %i)"
                          % (res["server"], res["APIversion"]))
                self.debug("Recieved greeting: %s" % res["greeting"])


    def get_cpu_topology(self):
        ''' figure out how many processors we have available'''

        topology = CPUtopology()
        topology.parse()

        self.numcores = topology.getCPUcores(True)
        self.debug("counted %d cores (%d online) and %d sockets" %
                   (topology.getCPUcores(False), self.numcores,
                    topology.getCPUsockets()))
        return topology.getXMLdata()

    def __get_services_sysvinit(self):
        reject = ('functions', 'halt', 'killall', 'single', 'linuxconf', 'kudzu',
                  'skeleton', 'README', '*.dpkg-dist', '*.dpkg-old', 'rc', 'rcS',
                  'single', 'reboot', 'bootclean.sh')
        for sdir in ('/etc/init.d', '/etc/rc.d/init.d'):
            if os.path.isdir(sdir):
                servicesdir = sdir
                break
        if not servicesdir:
            raise RuntimeError, "No services dir (init.d) found on your system"
        self.debug("Services located in %s, going through each service file to check status" % servicesdir)
        ret_services = {}
        for service in glob.glob(os.path.join(servicesdir, '*')):
            servicename = os.path.basename(service)
            if not [1 for p in reject if fnmatch.fnmatch(servicename, p)] and os.access(service, os.X_OK):
                cmd = '%s -qs "\(^\|\W\)status)" %s' % (getcmdpath('grep'), service)
                c = subprocess.Popen(cmd, shell=True)
                c.wait()
                if c.returncode == 0:
                    cmd = ['env', '-i', 'LANG="%s"' % os.environ['LANG'], 'PATH="%s"' % os.environ['PATH'], 'TERM="%s"' % os.environ['TERM'], service, 'status']
                    c = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                    c.wait()
                    if c.returncode == 0 and (c.stdout.read() or c.stderr.read()):
                        ret_services[servicename] = 'running'
                    else:
                        ret_services[servicename] = 'not running'
                else:
                    ret_services[servicename] = 'unknown'
        return ret_services

    def __get_services_systemd(self):
        ret_services = {}
        cmd = '%s list-unit-files -t service --no-legend' % getcmdpath('systemctl')
        self.debug("cmd: %s" % cmd)
        c = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        for p in c.stdout:
            # p are lines like "servicename.service status"
            v = p.strip().split()
            ret_services[v[0].split('.')[0]] = v[1]
        return ret_services

    def get_services(self):
        cmd = [getcmdpath('ps'), '-ocomm=',  '1']
        c = subprocess.Popen(cmd, stdout=subprocess.PIPE)
        self.init = c.stdout.read().strip()
        if self.init == 'systemd':
            self.debug("Using systemd to get services status")
            return self.__get_services_systemd()
        elif self.init == 'init':
            self.init = 'sysvinit'
            self.debug("Using sysvinit to get services status")
            return self.__get_services_sysvinit()
        else:
            raise RuntimeError, "Unknown init system (%s)" % self.init
        return {}

    def get_kthreads(self):
        policies = {'FF':'fifo', 'RR':'rrobin', 'TS':'other', '?':'unknown' }
        ret_kthreads = {}
        self.debug("getting kthread status")
        cmd = '%s -eocommand,pid,policy,rtprio,comm' % getcmdpath('ps')
        self.debug("cmd: %s" % cmd)
        c = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE)
        for p in c.stdout:
            v = p.strip().split()
            kcmd = v.pop(0)
            try:
                if int(v[0]) > 0 and kcmd.startswith('[') and kcmd.endswith(']'):
                    ret_kthreads[v[0]] = {'policy' : policies[v[1]],
                                          'priority' : v[2], 'name' : v[3] }
            except ValueError:
                pass    # Ignore lines which don't have a number in the first row
        return ret_kthreads

    def get_modules(self):
        modlist = []
        try:
            fp = open('/proc/modules', 'r')
            line = fp.readline()
            while line:
                mod = line.split()
                modlist.append({"modname": mod[0],
                                "modsize": mod[1],
                                "numusers": mod[2],
                                "usedby": mod[3],
                                "modstate": mod[4]})
                line = fp.readline()
            fp.close()
        except Exception, err:
            raise err
        return modlist

    def parse_options(self, cmdargs):
        '''parse the command line arguments'''
        parser = optparse.OptionParser()
        parser.add_option("-d", "--duration", dest="duration",
                          type="string", default=self.config.duration,
                          help="specify length of test run (default: %default)")
        parser.add_option("-v", "--verbose", dest="verbose",
                          action="store_true", default=self.config.verbose,
                          help="turn on verbose prints (default: %default)")
        parser.add_option("-w", "--workdir", dest="workdir",
                          type="string", default=self.workdir,
                          help="top directory for rteval data (default: %default)")
        parser.add_option("-l", "--loaddir", dest="srcdir",
                          type="string", default=self.config.srcdir,
                          help="directory for load source tarballs (default: %default)")
        parser.add_option("-i", "--installdir", dest="installdir",
                          type="string", default=self.config.installdir,
                          help="place to locate installed templates (default: %default)")
        parser.add_option("-s", "--sysreport", dest="sysreport",
                          action="store_true", default=self.config.sysreport,
                          help='run sysreport to collect system data (default: %default)')
        parser.add_option("-D", '--debug', dest='debugging',
                          action='store_true', default=self.config.debugging,
                          help='turn on debug prints (default: %default)')
        parser.add_option("-X", '--xmlrpc-submit', dest='xmlrpc',
                          action='store', default=self.config.xmlrpc, metavar='HOST',
                          help='Hostname to XML-RPC server to submit reports')
        parser.add_option("-P", "--xmlrpc-no-abort", dest="xmlrpc_noabort",
                          action='store_true', default=False,
                          help="Do not abort if XML-RPC server do not respond to ping request");
        parser.add_option("-Z", '--summarize', dest='summarize',
                          action='store_true', default=False,
                          help='summarize an already existing XML report')
        parser.add_option("-H", '--raw-histogram', dest='rawhistogram',
                          action='store_true', default=False,
                          help='Generate raw histogram data for an already existing XML report')
        parser.add_option("-f", "--inifile", dest="inifile",
                          type='string', default=None,
                          help="initialization file for configuring loads and behavior")
        parser.add_option("-a", "--annotate", dest="annotate",
                          type="string", default=None,
                          help="Add a little annotation which is stored in the report")
        parser.add_option("-L", "--logging", dest="logging",
                         action='store_true', default=False,
                         help='log the output of the loads in the report directory')
        parser.add_option("-O", "--onlyload", dest="onlyload",
                          action='store_true', default=False,
                          help="only run the loads (don't run measurement threads)")

        # module options
        parser.add_option("", "--cyclictest-interval", dest="cyclictest_interval",
                          action="store", type="int",
                          help="cyclictest measurement interval in microseconds")
        parser.add_option("", "--cyclictest-distance", dest="cyclictest_distance",
                          action="store", type="int",
                          help="cyclictest measurement interval increment in microseconds")
        parser.add_option("", "--cyclictest-buckets", dest="cyclictest_buckets",
                          action="store", type="int",
                          help="number of cyclictest 1 microsecond histogram buckets")
        parser.add_option("", "--cyclictest-priority", dest="cyclictest_priority",
                          action="store", type="int",
                          help="SCHED_FIFO priority of measurement threads")

        parser.add_option("", "--hackbench-jobspercore", dest="hackbench_jobspercore",
                          action="store", type="int",
                          help="number of hackbench jobs per-core")
        parser.add_option("", "--kcompile-jobspercore", dest="kcompile_jobspercore",
                          action="store", type="int",
                          help="number of kernel compile jobs per-core")


        (self.cmd_options, self.cmd_arguments) = parser.parse_args(args = cmdargs)
        if self.cmd_options.duration:
            mult = 1.0
            v = self.cmd_options.duration.lower()
            if v.endswith('s'):
                v = v[:-1]
            elif v.endswith('m'):
                v = v[:-1]
                mult = 60.0
            elif v.endswith('h'):
                v = v[:-1]
                mult = 3600.0
            elif v.endswith('d'):
                v = v[:-1]
                mult = 3600.0 * 24.0
            self.cmd_options.duration = float(v) * mult
        self.workdir = os.path.abspath(self.cmd_options.workdir)


    def debug(self, str):
        if self.config.debugging is True:
            print "rteval: %s" % str

    def info(self, str):
        if self.config.verbose is True:
            print str

    def run_sysreport(self):
        import glob
        if os.path.exists('/usr/sbin/sosreport'):
            exe = '/usr/sbin/sosreport'
        elif os.path.exists('/usr/sbin/sysreport'):
            exe = '/usr/sbin/sysreport'
        else:
            raise RuntimeError, "Can't find sosreport/sysreport"

        self.debug("report tool: %s" % exe)
        options =  ['-k', 'rpm.rpmva=off',
                    '--name=rteval',
                    '--batch',
                    '--no-progressbar']

        self.info("Generating SOS report")
        self.info("using command %s" % " ".join([exe]+options))
        subprocess.call([exe] + options)
        for s in glob.glob('/tmp/s?sreport-rteval-*'):
            self.debug("moving %s to %s" % (s, self.reportdir))
            shutil.move(s, self.reportdir)


    def genxml(self, duration, accum, samples, xslt = None):
        seconds = duration.seconds
        hours = seconds / 3600
        if hours: seconds -= (hours * 3600)
        minutes = seconds / 60
        if minutes: seconds -= (minutes * 60)
        (sys, node, release, ver, machine) = os.uname()

        # Start new XML report
        self.xmlreport = xmlout.XMLOut('rteval', self.version)
        self.xmlreport.NewReport()

        self.xmlreport.openblock('run_info', {'days': duration.days,
                                 'hours': hours,
                                 'minutes': minutes,
                                 'seconds': seconds})
        self.xmlreport.taggedvalue('date', self.start.strftime('%Y-%m-%d'))
        self.xmlreport.taggedvalue('time', self.start.strftime('%H:%M:%S'))
        if self.annotate:
            self.xmlreport.taggedvalue('annotate', self.annotate)
        self.xmlreport.closeblock()
        self.xmlreport.openblock('uname')
        self.xmlreport.taggedvalue('node', node)
        isrt = 1
        if ver.find(' RT ') == -1:
            isrt = 0
        self.xmlreport.taggedvalue('kernel', release, {'is_RT':isrt})
        self.xmlreport.taggedvalue('arch', machine)
        self.xmlreport.taggedvalue('baseos', self.baseos)
        self.xmlreport.closeblock()

        self.xmlreport.openblock("clocksource")
        self.xmlreport.taggedvalue('current', self.current_clocksource)
        self.xmlreport.taggedvalue('available', self.available_clocksource)
        self.xmlreport.closeblock()

        self.xmlreport.openblock('hardware')
        self.xmlreport.AppendXMLnodes(self.cputopology)
        self.xmlreport.taggedvalue('numa_nodes', self.numanodes)
        self.xmlreport.taggedvalue('memory_size', "%.3f" % self.memsize[0], {"unit": self.memsize[1]})
        self.xmlreport.closeblock()

        self.xmlreport.openblock('services', {'init': self.init})
        for s in self.services:
            self.xmlreport.taggedvalue("service", self.services[s], {"name": s})
        self.xmlreport.closeblock()

        keys = self.kthreads.keys()
        if len(keys):
            keys.sort()
            self.xmlreport.openblock('kthreads')
            for pid in keys:
                self.xmlreport.taggedvalue('thread', self.kthreads[pid]['name'],
                                           { 'policy' : self.kthreads[pid]['policy'],
                                             'priority' : self.kthreads[pid]['priority'],
                                             })
            self.xmlreport.closeblock()

        modlist = util.get_modules()
        if len(modlist):
            self.xmlreport.openblock('kernelmodules')
            for mod in modlist:
                self.xmlreport.openblock('module')
                self.xmlreport.taggedvalue('info', mod['modname'],
                                           {'size': mod['modsize'],
                                            'state': mod['modstate'],
                                            'numusers': mod['numusers']})
                if mod['usedby'] != '-':
                    self.xmlreport.openblock('usedby')
                    for ub in mod['usedby'].split(','):
                        if len(ub):
                            self.xmlreport.taggedvalue('module', ub, None)
                    self.xmlreport.closeblock()
                self.xmlreport.closeblock()
            self.xmlreport.closeblock()

        #
        # Retrieve configured IP addresses
        #
        self.xmlreport.openblock('network_config')

        # Get the interface name for the IPv4 default gw
        route = open('/proc/net/route')
        defgw4 = None
        if route:
            rl = route.readline()
            while rl != '' :
                rl = route.readline()
                splt = rl.split("\t")
                # Only catch default route
                if len(splt) > 2 and splt[2] != '00000000' and splt[1] == '00000000':
                    defgw4 = splt[0]
                    break
            route.close()

        # Make an interface tag for each device found
        if hasattr(ethtool, 'get_interfaces_info'):
            # Using the newer python-ethtool API (version >= 0.4)
            for dev in ethtool.get_interfaces_info(ethtool.get_devices()):
                if cmp(dev.device,'lo') == 0:
                    continue

                self.xmlreport.openblock('interface',
                                         {'device': dev.device,
                                          'hwaddr': dev.mac_address}
                                         )

                # Protcol configurations
                if dev.ipv4_address:
                    self.xmlreport.openblock('IPv4',
                                             {'ipaddr': dev.ipv4_address,
                                              'netmask': dev.ipv4_netmask,
                                              'broadcast': dev.ipv4_broadcast,
                                              'defaultgw': (defgw4 == dev.device) and '1' or '0'}
                                             )
                    self.xmlreport.closeblock()

                for ip6 in dev.get_ipv6_addresses():
                    self.xmlreport.openblock('IPv6',
                                             {'ipaddr': ip6.address,
                                              'netmask': ip6.netmask,
                                              'scope': ip6.scope}
                                             )
                    self.xmlreport.closeblock()
                self.xmlreport.closeblock()
        else: # Fall back to older python-ethtool API (version < 0.4)
            ifdevs = ethtool.get_active_devices()
            ifdevs.remove('lo')
            ifdevs.sort()

            for dev in ifdevs:
                self.xmlreport.openblock('interface',
                                         {'device': dev,
                                          'hwaddr': ethtool.get_hwaddr(dev)}
                                         )
                self.xmlreport.openblock('IPv4',
                                         {'ipaddr': ethtool.get_ipaddr(dev),
                                          'netmask': ethtool.get_netmask(dev),
                                          'defaultgw': (defgw4 == dev) and '1' or '0'}
                                         )
                self.xmlreport.closeblock()
                self.xmlreport.closeblock()
        self.xmlreport.closeblock()

        self.xmlreport.openblock('loads', {'load_average':str(accum / samples)})
        for load in self.loads:
            load.genxml(self.xmlreport)
        self.xmlreport.closeblock()
        self.cyclictest.genxml(self.xmlreport)

        # now generate the dmidecode data for this host
        d = dmi.DMIinfo(self.config.GetSection('rteval'))
        d.genxml(self.xmlreport)

        # Close the report - prepare for return the result
        self.xmlreport.close()

        # Write XML (or write XSLT parsed XML if xslt != None)
        if self.xml != None:
            self.xmlreport.Write(self.xml, xslt)
        else:
            # If no file is set, use stdout
            self.xmlreport.Write("-", xslt) # libxml2 defines a filename as "-" to be stdout


    def report(self):
        "Create a screen report, based on a predefined XSLT template"
        self.xmlreport.Write("-", self.config.xslt_report)

    def XMLreport(self):
        "Retrieves the complete rteval XML report as a libxml2.xmlDoc object"
        return self.xmlreport.GetXMLdocument()

    def show_report(self, xmlfile, xsltfile):
        '''summarize a previously generated xml file'''
        print "Loading %s for summarizing" % xmlfile

        xsltfullpath = os.path.join(self.config.installdir, xsltfile)
        if not os.path.exists(xsltfullpath):
            raise RuntimeError, "can't find XSL template (%s)!" % xsltfullpath

        xmlreport = xmlout.XMLOut('rteval', self.version)
        xmlreport.LoadReport(xmlfile)
        xmlreport.Write('-', xsltfullpath)
        del xmlreport

    def start_loads(self):
        if len(self.loads) == 0:
            raise RuntimeError, "start_loads: No loads defined!"
        self.info ("starting loads:")
        for l in self.loads:
            l.start()
        # now wait until they're all ready
        self.info("waiting for ready from all loads")
        ready=False
        while not ready:
            busy = 0
            for l in self.loads:
                if not l.isAlive():
                    raise RuntimeError, "%s died" % l.name
                if not l.isReady():
                    busy += 1
                    self.debug("waiting for %s" % l.name)
            if busy:
                time.sleep(1.0)
            else:
                ready = True

    def stop_loads(self):
        if len(self.loads) == 0:
            raise RuntimeError, "stop_loads: No loads defined!"
        self.info("stopping loads: ")
        for l in self.loads:
            self.info("\t%s" % l.name)
            l.stopevent.set()
            l.join(2.0)

    def make_report_dir(self):
        t = self.start
        i = 1
        self.reportdir = os.path.join(self.workdir,
                                      t.strftime("rteval-%Y%m%d-"+str(i)))
        while os.path.exists(self.reportdir):
            i += 1
            self.reportdir = os.path.join(self.workdir,
                                          t.strftime('rteval-%Y%m%d-'+str(i)))
        if not os.path.isdir(self.reportdir):
            os.mkdir(self.reportdir)
            os.mkdir(os.path.join(self.reportdir, "logs"))
        return self.reportdir

    def get_dmesg(self):
        dpath = "/var/log/dmesg"
        if not os.path.exists(dpath):
            print "dmesg file not found at %s" % dpath
            return
        shutil.copyfile(dpath, os.path.join(self.reportdir, "dmesg"))


    def show_remaining_time(self, remaining):
        r = int(remaining)
        days = r / 86400
        if days: r = r - (days * 86400)
        hours = r / 3600
        if hours: r = r - (hours * 3600)
        minutes = r / 60
        if minutes: r = r - (minutes * 60)
        print "rteval time remaining: %d days, %d hours, %d minutes, %d seconds" % (days, hours, minutes, r)


    def measure(self):
        # Collect misc system info
        self.baseos = util.get_base_os()
        self.cputopology = self.get_cpu_topology()
        self.numanodes = util.get_num_nodes()
        self.memsize = util.get_memory_size()
        (self.current_clocksource, self.available_clocksource) = util.get_clocksources()
        self.services = self.get_services()
        self.kthreads = self.get_kthreads()

        onlyload = self.cmd_options.onlyload

        builddir = os.path.join(self.workdir, 'rteval-build')
        if not os.path.isdir(builddir): os.mkdir(builddir)
        self.reportfile = os.path.join(self.reportdir, "summary.rpt")
        self.xml = os.path.join(self.reportdir, "summary.xml")

        # read in loads from the ini file
        self.load_modules = []
        loads = self.config.GetSection("loads")
        for l in loads:
            # hope to eventually have different kinds but module is only on
            # for now (jcw)
            if l[1].lower() == 'module':
                self.info("importing load module %s" % l[0])
                self.load_modules.append(__import__(l[0]))

        self.info("setting up loads")
        self.loads = []
        params = {'workdir':self.workdir,
                  'reportdir':self.reportdir,
                  'builddir':builddir,
                  'srcdir':self.config.srcdir,
                  'verbose': self.config.verbose,
                  'debugging': self.config.debugging,
                  'numcores':self.numcores,
                  'logging':self.config.logging,
                  'memsize':self.memsize,
                  'numanodes':self.numanodes,
                  'duration':self.config.duration,
                  }

        for m in self.load_modules:
            self.config.AppendConfig(m.__name__, params)
            self.info("creating load instance for %s" % m.__name__)
            self.loads.append(m.create(self.config.GetSection(m.__name__)))

        if not onlyload:
            self.config.AppendConfig('cyclictest', params)
            self.info("setting up cyclictest")
            self.cyclictest = cyclictest.Cyclictest(params=self.config.GetSection('cyclictest'))

        nthreads = 0
        try:
            # start the loads
            self.start_loads()

            print "rteval run on %s started at %s" % (os.uname()[2], time.asctime())
            print "started %d loads on %d cores" % (len(self.loads), self.numcores),
            if self.numanodes > 1:
                print " with %d numa nodes" % self.numanodes
            else:
                print ""
            print "Run duration: %d seconds" % self.config.duration

            start = datetime.now()

            if not onlyload:
                # start the cyclictest thread
                self.info("starting cyclictest")
                self.cyclictest.start()

            # turn loose the loads
            self.info("sending start event to all loads")
            for l in self.loads:
                l.startevent.set()
                nthreads += 1

            accum = 0.0
            samples = 0

            report_interval = int(self.config.GetSection('rteval').report_interval)

            # wait for time to expire or thread to die
            signal.signal(signal.SIGINT, sigint_handler)
            signal.signal(signal.SIGTERM, sigterm_handler)
            self.info("waiting for duration (%f)" % self.config.duration)
            stoptime = (time.time() + self.config.duration)
            currtime = time.time()
            rpttime = currtime + report_interval
            loadcount = 5
            while (currtime <= stoptime) and not sigint_received:
                time.sleep(1.0)
                if not onlyload and not self.cyclictest.isAlive():
                    raise RuntimeError, "cyclictest thread died!"
                if len(threading.enumerate()) < nthreads:
                    raise RuntimeError, "load thread died!"
                if not loadcount:
                    # open the loadavg /proc entry
                    p = open("/proc/loadavg")
                    load = float(p.readline().split()[0])
                    p.close()
                    accum += load
                    samples += 1
                    loadcount = 5
                    #self.debug("current loadavg: %f, running avg: %f (load: %f, samples: %d)" % \
                    #               (load, accum/samples, load, samples))
                else:
                    loadcount -= 1
                if currtime >= rpttime:
                    left_to_run = stoptime - currtime
                    self.show_remaining_time(left_to_run)
                    rpttime = currtime + report_interval
                    print "load average: %.2f" % (accum / samples)
                currtime = time.time()
            self.debug("out of measurement loop")
            signal.signal(signal.SIGINT, signal.SIG_DFL)
            signal.signal(signal.SIGTERM, signal.SIG_DFL)

        except RuntimeError, e:
            print "Runtime error during measurement: %s", e
            raise

        finally:
            if not onlyload:
                # stop cyclictest
                self.cyclictest.stopevent.set()

            # stop the loads
            self.stop_loads()

        print "stopping run at %s" % time.asctime()
        if not onlyload:
            # wait for cyclictest to finish calculating stats
            self.cyclictest.finished.wait()
            self.genxml(datetime.now() - start, accum, samples)
            self.report()
            if self.config.sysreport:
                self.run_sysreport()


    def XMLRPC_Send(self):
        "Sends the report to a given XML-RPC host.  Returns 0 on success or 2 on submission failure."

        if not self.config.xmlrpc:
            return 2

        url = "http://%s/rteval/API1/" % self.config.xmlrpc
        attempt = 0
        exitcode = 2   # Presume failure
        warning_sent = False
        while attempt < 6:
            try:
                client = rtevalclient.rtevalclient(url)
                print "Submitting report to %s" % url
                rterid = client.SendReport(self.xmlreport.GetXMLdocument())
                print "Report registered with submission id %i" % rterid
                attempt = 10
                exitcode = 0 # Success
            except socket.error:
                if (self.mailer is not None) and (not warning_sent):
                    self.mailer.SendMessage("[RTEVAL:WARNING] Failed to submit report to XML-RPC server",
                                            "Server %s did not respond.  Not giving up yet."
                                            % self.config.xmlrpc)
                    warning_sent = True

                attempt += 1
                if attempt > 5:
                    break # To avoid sleeping before we abort

                print "Failed sending report.  Doing another attempt(%i) " % attempt
                time.sleep(attempt*5*60) # Incremental sleep - sleep attempts*5 minutes

            except Exception, err:
                raise err

        if (self.mailer is not None):
            # Send final result messages
            if exitcode == 2:
                self.mailer.SendMessage("[RTEVAL:FAILURE] Failed to submit report to XML-RPC server",
                                        "Server %s did not respond at all after %i attempts."
                                        % (self.config.xmlrpc, attempt - 1))
            elif (exitcode == 0) and warning_sent:
                self.mailer.SendMessage("[RTEVAL:SUCCESS] XML-RPC server available again",
                                        "Succeeded to submit the report to %s in the end."
                                        % (self.config.xmlrpc))
        return exitcode


    def tar_results(self):
        if not os.path.isdir(self.reportdir):
            raise RuntimeError, "no such directory: %s" % self.reportdir
        import tarfile
        dirname = os.path.dirname(self.reportdir)
        rptdir = os.path.basename(self.reportdir)
        cwd = os.getcwd()
        os.chdir(dirname)
        try:
            t = tarfile.open(rptdir + ".tar.bz2", "w:bz2")
            t.add(rptdir)
            t.close()
        except:
            os.chdir(cwd)

    def summarize(self, file):
        isarchive = False
        summary = file
        if file.endswith(".tar.bz2"):
            import tarfile
            try:
                t = tarfile.open(file)
            except:
                print "Don't know how to summarize %s (tarfile open failed)" % file
                return
            element = None
            for f in t.getnames():
                if f.find('summary.xml') != -1:
                    element = f
                    break
            if element == None:
                print "No summary.xml found in tar archive %s" % file
                return
            tmp = tempfile.gettempdir()
            self.debug("extracting %s from %s for summarizing" % (element, file))
            t.extract(element, path=tmp)
            summary = os.path.join(tmp, element)
            isarchive = True
        self.show_report(summary, 'rteval_text.xsl')
        if isarchive:
            os.unlink(summary)

    def rteval(self):
        ''' main function for rteval'''
        retval = 0;

        # Parse initial DMI decoding errors
        dmi.ProcessWarnings()

        # if --summarize was specified then just parse the XML, print it and exit
        if self.cmd_options.summarize or self.cmd_options.rawhistogram:
            if len(self.cmd_arguments) < 1:
                raise RuntimeError, "Must specify at least one XML file with --summarize!"

            for x in self.cmd_arguments:
                if self.cmd_options.summarize:
                    self.summarize(x)
                elif self.cmd_options.rawhistogram:
                    self.show_report(x, 'rteval_histogram_raw.xsl')

            sys.exit(0)

        if os.getuid() != 0:
            print "Must be root to run rteval!"
            sys.exit(-1)

        self.debug('''rteval options:
        workdir: %s
        loaddir: %s
        reportdir: %s
        verbose: %s
        debugging: %s
        logging:  %s
        duration: %f
        sysreport: %s
        inifile:  %s''' % (self.workdir, self.config.srcdir, self.reportdir, self.config.verbose,
                           self.config.debugging, self.config.logging, self.config.duration,
                           self.config.sysreport, self.inifile))

        if not os.path.isdir(self.workdir):
            raise RuntimeError, "work directory %d does not exist" % self.workdir

        # create our report directory
        try:
            self.make_report_dir()
        except:
            print "Cannot create the report dir!"
            print "(is this an NFS filesystem with rootsquash turned on?)"
            sys.exit(-1)

        self.measure()

        # if --xmlrpc-submit | -X was given, send our report to this host
        if self.config.xmlrpc:
            retval = self.XMLRPC_Send()

        self.get_dmesg()
        self.tar_results()

        return retval

if __name__ == '__main__':
    import pwd, grp

    try:
        # Parse initial DMI decoding errors
        dmi.ProcessWarnings()

        rteval = RtEval(sys.argv[1:])
        ec = rteval.rteval()
        rteval.debug("exiting with exit code: %d" % ec)
        sys.exit(ec)
    except KeyboardInterrupt:
        sys.exit(0)
