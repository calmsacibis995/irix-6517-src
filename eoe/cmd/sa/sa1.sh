#! /bin/sh
#	Copyright (c) 1984 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

# #ident	"@(#)sa:sa1.sh	1.4"
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sa/RCS/sa1.sh,v 1.5 1993/07/06 22:56:24 raghav Exp $"
#	sa1.sh 1.4 of 5/13/85
DATE=`date +%d`
ENDIR=/usr/lib/sa
DFILE=/var/adm/sa/sa$DATE
cd $ENDIR
if [ $# = 0 ]
then
	exec $ENDIR/sadc 1 1 $DFILE
else
	exec $ENDIR/sadc $* $DFILE
fi
