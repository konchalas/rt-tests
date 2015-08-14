#!/usr/bin/python -tt
#
# platform utility functions used by various parts of rteval
#

import sys
import os
import os.path
import subprocess

def get_base_os():
    '''record what userspace we're running on'''
    distro = "unknown"
    for f in ('redhat-release', 'fedora-release'):
        p = os.path.join('/etc', f)
        if os.path.exists(p):
            f = open(p, 'r')
            distro = f.readline().strip()
            f.close()
            break
    return distro

def get_num_nodes():
    from glob import glob
    nodes = len(glob('/sys/devices/system/node/node*'))
    return nodes
        
def get_memory_size():
    '''find out how much memory is installed'''
    f = open('/proc/meminfo')
    rawsize = 0
    for l in f:
        if l.startswith('MemTotal:'):
            parts = l.split()
            if parts[2].lower() != 'kb':
                raise RuntimeError, "Units changed from kB! (%s)" % parts[2]
            rawsize = int(parts[1])
            f.close()
            break
    if rawsize == 0:
        raise RuntimeError, "can't find memtotal in /proc/meminfo!"

    # Get a more readable result
    # Note that this depends on  /proc/meminfo starting in Kb
    units = ('KB', 'MB','GB','TB')
    size = rawsize
    for unit in units:
        if size < 1024:
            break
        size = float(size) / 1024
    return (size, unit)


def get_clocksources():
    '''get the available and curent clocksources for this kernel'''
    path = '/sys/devices/system/clocksource/clocksource0'
    if not os.path.exists(path):
        raise RuntimeError, "Can't find clocksource path in /sys"
    f = open (os.path.join (path, "current_clocksource"))
    current_clocksource = f.readline().strip()
    f = open (os.path.join (path, "available_clocksource"))
    available_clocksource = f.readline().strip()
    f.close()
    return (current_clocksource, available_clocksource)


def get_modules():
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


if __name__ == "__main__":
    print "\tRunning on %s" % get_base_os()
    print "\tNUMA nodes: %d" % get_num_nodes()
    print "\tMemory available: %03.2f %s" % get_memory_size()
    (curr, avail) = get_clocksources()
    print "\tCurrent clocksource: %s" % curr
    print "\tAvailable clocksources: %s" % avail
    print "\tModules:"
    for m in get_modules():
        print "\t\t%s: %s" % (m['modname'], m['modstate'])
    
