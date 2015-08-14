#!/usr/bin/env python
# Copyright (c) 2010 Red Hat, Inc. All rights reserved. This copyrighted material
# is made available to anyone wishing to use, modify, copy, or
# redistribute it subject to the terms and conditions of the GNU General
# Public License v.2.
#
# This program is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# Author: David Sommerseth <davids@redhat.com>
#

import os, sys

class UnitTest(object):
    "Unified unit test class"

    def __init__(self, srcrootdir):
        "UnitTest constructor.  srcrootdir argument must point at the source root directory"
        self.imported_mods = []
        self.mod_impcount = 0
        self.mod_impfail = 0
        self.mod_testpass = 0
        self.mod_testfail = 0
        self.mod_testmiss = 0
        self.rootdir = srcrootdir
        sys.path.insert(0, self.rootdir)


    def LoadModules(self, modules):
        """Loads all the defined modules.  The modules argument takes a tuple list
 consisting of ('subdir','module name')"""

        for (directory, mod) in modules:
            # Check if the directory is in the "include" path
            try:
                sys.path.index('%s/%s' % (self.rootdir,directory))
            except ValueError:
                # Not found, insert it
                sys.path.insert(0, '%s/%s' % (self.rootdir, directory))

            try:
                impmod = __import__(mod)
                print "** Imported %s/%s" % (directory, mod)
                self.imported_mods.append({'name': '%s/%s' %(directory, mod),
                                           'module':  impmod})
                self.mod_impcount += 1
            except ImportError, e:
                print "!! ** ERROR ** Failed to import %s/%s (Exception: %s)" % (directory, mod, str(e))
                self.mod_impfail += 1

        return True


    def RunTests(self):
        "Runs the unit_test() function in all successfully imported modules"

        for m in self.imported_mods:
            try:
                # Check if the unit_test() function exists and is callable before trying
                # to run the unit test
                if callable(m['module'].unit_test):
                    print
                    print 78 * '-'
                    print "** Running unit test for: %s" % m['name']
                    print 78 * '.'
                    res = m['module'].unit_test(self.rootdir)
                    print 78 * '.'
                    if res == 0:
                        print "** Result of %s: PASS" % m['name']
                        self.mod_testpass += 1
                    else:
                        print "** Result of %s: FAILED (return code: %s)" % (m['name'], str(res))
                        self.mod_testfail += 1
                    print 78 * '='
                else:
                    self.mod_testmiss += 1
                    print "!!! ** ERROR **  Could not run %s::unit_test()" % m['name']
            except AttributeError:
                self.mod_testmiss += 1
                print "!!! ** ERROR **  No %s::unit_test() method found" % m['name']


    def PrintTestSummary(self):
        "Prints a result summary of all the tests"
        print
        print " --------------------"
        print "  ** TEST SUMMARY ** "
        print " --------------------"
        print
        print "  - Modules:"
        print "      Declared for test:      %i" % (self.mod_impcount + self.mod_impfail)
        print "      Successfully imported:  %i" % self.mod_impcount
        print "      Failed import:          %i" % self.mod_impfail
        print
        print "  - Tests:"
        print "      Tests scheduled:        %i" % (self.mod_testpass + self.mod_testfail + self.mod_testmiss)
        print "      Sucessfully tests:      %i" % self.mod_testpass
        print "      Failed tests:           %i" % self.mod_testfail
        print "      Missing unit_test()     %i" % self.mod_testmiss
        print


if __name__ == '__main__':

    # Retrieve the root directory if the source dir
    # - use the first occurence of the 'v7' subdir as the root dirq
    srcrootdir_ar = os.getcwd().split('/')
    rootdir = '/'.join(srcrootdir_ar[0:srcrootdir_ar.index('rteval')+1])
    print "** Source root dir: %s" % rootdir

    # Prepare the unit tester
    tests = UnitTest(rootdir)

    # Load defined modules  ('subdir','import name')
    tests.LoadModules((
            ('rteval','cputopology'),
            ('rteval','dmi'),
            ('rteval','rtevalConfig'),
            ('rteval','xmlout'),
            ('server','unittest')
            ))
    # Run all tests
    tests.RunTests()
    tests.PrintTestSummary()

