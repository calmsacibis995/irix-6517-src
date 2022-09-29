#! /sbin/sh
#	Copyright (c) 1984 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)fstyp:fstyp.sh	1.2"
#ident	"$Revision: 1.7 $"
#
#	Determine the fs identifier of a file system.
#
#!	chmod +x ${file}

if [ $# -ne 1 ]
then
	echo "Usage: fstyp special"
	exit 1
fi

if [ ! -r $1 ]
then
	echo "fstyp: cannot read <$1>"
	exit 1
fi

#
#	Execute all heuristic functions in /sbin/fstyp.d and
#	return the fs identifier of the specified file system.
#

for f in /sbin/fstyp.d/*
do
	msg=`$f $1 2>&1`
	if [ $? -eq 0 ]
	then
		echo ${msg}
		exit 0
	fi
done
echo "Unknown_fstyp"
exit 1
