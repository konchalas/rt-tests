#
#   rteval - script for evaluating platform suitability for RT Linux
#
#           This program is used to determine the suitability of
#           a system for use in a Real Time Linux environment.
#           It starts up various system loads and measures event
#           latency while the loads are running. A report is generated
#           to show the latencies encountered during the run.
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
import os
import ConfigParser

class rtevalCfgSection(object):

    def __init__(self, section_cfg):
        self.__update_config_vars(section_cfg)
        self.__iter_list = None

    def __str__(self):
        "Simple method for dumping config when object is used as a string"
        return str(self.__cfgdata)

    def __update_config_vars(self, section_cfg):
        if section_cfg is None:
            return
        self.__cfgdata = section_cfg

        # create member variables from config info
        for m in section_cfg.keys():
            self.__dict__[m] = section_cfg[m]


    def __iter__(self):
        "Initialises for an iterator loop"
        self.__iter_list = self.keys()
        return self


    def next(self):
        "Function used by the iterator"
        if len(self.__iter_list) == 0:
            raise StopIteration
        else:
            elmt = self.__iter_list.pop()
            return (elmt, self.__cfgdata[elmt])


    def has_key(self, key):
        "has_key() wrapper for the configuration data"
        return self.__cfgdata.has_key(key)


    def keys(self):
        "keys() wrapper for configuration data"
        return self.__cfgdata.keys()

    def setdefault(self, key, defvalue):
        if not self.__dict__.has_key(key):
            self.__dict__[key] = defvalue
        return self.__dict__[key]

class rtevalConfig(rtevalCfgSection):
    "Config parser for rteval"

    def __init__(self, initvars = None, logfunc = None):
        self.__config_data = initvars or {}
        self.__config_files = []

        # export the rteval section to member variables, if section is found
        try:
            self._rtevalCfgSection__update_config_vars(self.__config_data['rteval'])
        except KeyError:
            pass  # If 'rteval' is not found, KeyError is raised and that's okay to ignore
        except Exception, err:
            raise err # All other errors will be passed on

        self.__info = logfunc or self.__nolog


    def __str__(self):
        "Simple method for dumping config when object is used as a string"
        return str(self.__config_data)


    def __nolog(self, str):
        "Dummy log function, used when no log function is configured"
        pass


    def __find_config(self):
        "locate a config file"

        for f in ('rteval.conf', '/etc/rteval.conf'):
            p = os.path.abspath(f)
            if os.path.exists(p):
                self.__info("found config file %s" % p)
                return p
        raise RuntimeError, "Unable to find configfile"


    def Load(self, fname = None, append = False):
        "read and parse the configfile"

        try:
            cfgfile = fname or self.__find_config()
        except:
            self.__info("no config file")
            return

        if self.ConfigParsed(cfgfile) is True:
            # Don't try to reread this file if it's already been parsed
            return

        self.__info("reading config file %s" % cfgfile)
        ini = ConfigParser.ConfigParser()
        ini.read(cfgfile)

        # wipe any previously read config info (other than the rteval stuff)
        if not append:
            for s in self.__config_data.keys():
                if s == 'rteval':
                    continue
                self.__config_data[s] = {}

        # copy the section data into the __config_data dictionary
        for s in ini.sections():
            if not self.__config_data.has_key(s):
                self.__config_data[s] = {}
            for i in ini.items(s):
                self.__config_data[s][i[0]] = i[1]

        # export the rteval section to member variables
        try:
            self._rtevalCfgSection__update_config_vars(self.__config_data['rteval'])
        except KeyError:
            pass
        except Exception, err:
            raise err

        # Register the file as read
        self.__config_files.append(cfgfile)
        return cfgfile

    def ConfigParsed(self, fname):
        "Returns True if the config file given by name has already been parsed"
        return self.__config_files.__contains__(fname)


    def AppendConfig(self, section, cfgvars):
        "Add more config parameters to a section.  cfgvard must be a dictionary of parameters"

        if type(cfgvars) is dict:
            for o in cfgvars.keys():
                self.__config_data[section][o] = cfgvars[o]
        else:
            for o in cfgvars.__dict__.keys():
                self.__config_data[section][o] = cfgvars.__dict__[o]

        if section == 'rteval':
            self._rtevalCfgSection__update_config_vars(self.__config_data['rteval'])


    def HasSection(self, section):
        return self.__config_data.has_key(section)


    def GetSection(self, section):
        try:
            # Return a new object with config settings of a given section
            return rtevalCfgSection(self.__config_data[section])
        except KeyError, err:
            raise KeyError("The section '%s' does not exist in the config file" % section)


def unit_test(rootdir):
    try:
        cfg = rtevalConfig()
        cfg.Load(rootdir + '/rteval/rteval.conf')
        print cfg
        return 0
    except Exception, e:
        print "** EXCEPTION %s", str(e)
        return 1


if __name__ == '__main__':
    import sys
    sys.exit(unit_test('..'))
