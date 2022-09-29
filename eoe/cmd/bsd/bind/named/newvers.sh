#!/bin/sh -
#
# Copyright (c) 1987 Regents of the University of California.
# All rights reserved.
#
# Redistribution and use in source and binary forms are permitted
# provided that this notice is preserved and that due credit is given
# to the University of California at Berkeley. The name of the University
# may not be used to endorse or promote products derived from this
# software without specific prior written permission. This software
# is provided ``as is'' without express or implied warranty.
#
#	@(#)newvers.sh	4.4 (Berkeley) 3/28/88
#
rm -f version.[oc]
t=`date '+(%D %R)'`
sed -e "s|%WHEN%|${t}|" -e "s|%WHOANDWHERE%|IRIX ${1}|"  -e "s|%VERSION%|4.9.7|" < Version.c >version.c
