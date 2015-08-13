#!/bin/bash

# (C) 2008 Sven-Thorsten Dietrich
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
# cyclictest_run:
# Execute modified cyclictest to collect histogram for gnuplot
#
# Usage: cyclictest_run [iteration] > histogram_file

NLOOPS=$1

#echo "usage: $0 [max iterations]" >&2
#exit 1

if [ $NLOOPS -gt 0 ]; then
	NLOOPS="-l $NLOOPS"
fi

CPUS=`getconf _NPROCESSORS_ONLN`
echo "#" `grep -m 1 "model name" /proc/cpuinfo`
echo "# CPUS: " $CPUS
echo "# Kernel: " `uname -rvm`
echo "# Iterations" $NLOOPS

n=0
for clocksource in /sys/devices/system/clocksource/clocksource*/current_clocksource; do
	echo "# Clocksource #$n: " `cat $clocksource`
	n=$(( $n + 1 ))
done

/usr/bin/time cyclictest -n -q -p 99 -a -t -D 1000 -i 250 -h 2000 -m 
