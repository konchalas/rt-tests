#!/usr/bin/python
from distutils.sysconfig import get_python_lib
from distutils.core import setup
from os.path import isfile, join
import glob
import os

# Get PYTHONLIB with no prefix so --prefix installs work.
PYTHONLIB = join(get_python_lib(standard_lib=1, prefix=''), 'site-packages')

setup(name="rteval",
      version = "1.38",
      description = "evaluate system performance for Realtime",
      author = "Clark Williams",
      author_email = "williams@redhat.com",
      license = "GPLv2",
      long_description =
"""\
The rteval script is used to judge the behavior of a hardware
platform while running a Realtime Linux kernel under a moderate
to heavy load.

Provides control logic for starting a system load and then running a
response time measurement utility (cyclictest) for a specified amount
of time. When the run is finished, the sample data from cyclictest is
analyzed for standard statistical measurements (i.e mode, median, range,
mean, variance and standard deviation) and a report is generated.
""",
      packages = ["rteval"],
      )
