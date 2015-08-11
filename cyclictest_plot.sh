#!/bin/bash
# 
# (C) 2008 Sven-Thorsten Dietrich <sdietrich@suse.de>
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

# Simple plotter of Cyclictest latency data on the host system.
#
# This program requires a patches version of cyclictest, 
# that embeds specific data in the file.
# Parameters: 
# 1: Character string reporting preemption configuration in the 
#    kernel. If none, use the one reported in /proc/config.gz
# 2: Max X-Axis value to plot. Use -1 for plotting max latency recorded

reqcmd()
{
	if ! which "$1" > /dev/null 2>&1; then
		echo "\"$1\" is required" >&2
		exit 1
	fi;
}

reqcmd gnuplot 

PLTFILE=$1
if [ -z "${PLTFILE}" ]; then 
	echo "usage: $0 <histogram file> [(Max of X-Axis / auto-scale: -1)]" >&2
	exit 1
fi

#PLT_STYLE=filledcurves
PLT_STYLE=lines
PLT_TITLE=`grep "# model name" $PLTFILE | sed "s/# model name : //"`
PLT_SYS=`grep " Kernel" $PLTFILE | sed "s/# Kernel: //"`
PLT_CPUS=`grep "CPUS" $PLTFILE | sed "s/# CPUS://"`
PLT_MAX=`grep "Max Latency" $PLTFILE | awk '{print $4}'`
PLT_XMAX=`grep "Max Latency" $PLTFILE | awk '{print $6}'`
PLT_SAMPLES=`grep "Iterations" $PLTFILE | awk '{print $3}'`

if [ $# -gt 1 ]; then 
    PLT_XMAX=$2
fi


# Auto scale - add 50us so last bucket get not coverd by plot-border
if [ $PLT_XMAX -lt 0 ]; then 
    PLT_XMAX=$(( $PLT_MAX + 50 ))
fi

# Scale the Y axis down by 2 log units.
if [ $PLT_SAMPLES -gt 0 ]; then
	let "PLT_YMAX=$PLT_SAMPLES/100"
else
	PLT_YMAX=""
fi

echo "CPUS:" $PLT_CPUS " Title: " $PLT_TITLE "Kernel: " $PLT_SYS "L-Max: " $PLT_MAX "X-Max" $PLT_XMAX "Y-Max" $PLT_YMAX


echo "Drawing ... ";

PLOTCMD="plot [0:$PLT_XMAX][0.1:$PLT_YMAX] \"${PLTFILE}\" using 2 title \"CPU 1\" with $PLT_STYLE";

if [ $PLT_CPUS -gt 1 ]
then
for i in `seq 2 $PLT_CPUS`;
   do
	let "PLT_COL=$i+1";
	PLOTCMD=$PLOTCMD", \"${PLTFILE}\" using "$PLT_COL" title \"CPU ${i}\" with $PLT_STYLE";
   done
fi


echo "
set xlabel \"Preemption Time (us)\"
set ylabel \"Number of Samples\"
set logscale y
set xrange [0:150]
set grid
show grid
set xtic auto
set ytic auto
set title \"${PLT_TITLE}\n${PLT_SYS}\"
set terminal pdf
set output \"${PLTFILE}.pdf\"
$PLOTCMD" | gnuplot - > /dev/null || exit 1

echo "Histogram created: ${PLTFILE}.png"

echo 0
