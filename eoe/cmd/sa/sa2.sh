#! /bin/sh
#	Copyright (c) 1984 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

# #ident	"@(#)sa:sa2.sh	1.3"
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sa/RCS/sa2.sh,v 1.7 1993/07/06 22:56:34 raghav Exp $"
#	sa2.sh 1.3 of 5/13/85
DATE=`date +%d`
RPT=/var/adm/sa/sar$DATE
DFILE=/var/adm/sa/sa$DATE

cd /var/adm/sa
/usr/bin/sar $* -f $DFILE > $RPT
find /var/adm/sa \( -name 'sar*' -o -name 'sa*' \) -type f -mtime +7 -exec rm {} \;
