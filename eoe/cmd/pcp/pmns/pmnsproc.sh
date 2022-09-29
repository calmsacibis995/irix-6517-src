#!/bin/sh
#
# Copyright 1997, Silicon Graphics, Inc.
# ALL RIGHTS RESERVED
# 
# UNPUBLISHED -- Rights reserved under the copyright laws of the United
# States.   Use of a copyright notice is precautionary only and does not
# imply publication or disclosure.
# 
# U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
# Use, duplication or disclosure by the Government is subject to restrictions
# as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
# in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
# in similar or successor clauses in the FAR, or the DOD or NASA FAR
# Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
# 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
# 
# THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
# INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
# DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
# PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
# GRAPHICS, INC.
#Tag 0x00010D13
#
# common procedures for the PMNS scripts
#
# $Revision: 1.7 $

# Some degree of paranoia here ...
PATH=/usr/sbin:/usr/bsd:/sbin:/usr/bin:/bin:/usr/pcp/bin
export PATH

# find cpp
#
CPP=''
for file in /lib/cpp /usr/lib/cpp \
	    /usr/cpu/sysgen/root/lib/cpp \
	    /usr/cpu/sysgen/root/usr/lib/cpp
do
    if [ -x $file ]
    then
	CPP=$file
	break
    fi
done
if [ -z "$CPP" ]
then
    echo "$prog: cannot locate \"cpp\", cannot proceed"
    exit
fi

# add -Uname controls to avoid translating any valid PCP name component
# ... see cpp(1)
#
CPP="$CPP -Usgi -Uunix -Umips"

# Note.  if someone fakes out pmbrand, this test will produce
#	 the wrong result, but other things down stream will not work
#
# Note.	if changes made here, make sure __check_license() in
#	/usr/pcp/lib/pmdaproc.sh reflects the same changes
#
_can_load_ascii()
{
    __status=0
    __PMBRAND=/usr/pcp/bin/pmbrand
    [ ! -x $__PMBRAND ] && __PMBRAND=$TOOLROOT/usr/pcp/bin/pmbrand
    if [ ! -x $__PMBRAND ]
    then
	echo "pmnsproc.sh: Warning: pmbrand not found in any of the expected places"
    else
	rm -f /tmp/.$$.check
	$__PMBRAND -l 2>&1 | sed '/^Licenses /d' >/tmp/.$$.check
	if fgrep -s 'PCP' /tmp/.$$.check
	then
	    :
	else
	    fgrep -s 'WEBPERF
WebMeter' /tmp/.$$.check && __status=1
	fi
	rm -f /tmp/.$$.check
    fi
    return $__status
}
