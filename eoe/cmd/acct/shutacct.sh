#!/sbin/sh
#	Copyright (c) 1993 UNIX System Laboratories, Inc.
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF
#	UNIX System Laboratories, Inc.   	
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#	copyright	"%c%"

#ident	"@(#)acct:common/cmd/acct/shutacct.sh	1.6.1.4"
#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/acct/RCS/shutacct.sh,v 1.3 1993/11/05 04:28:14 jwag Exp $"
#	"shutacct [arg] - shuts down acct, called from /usr/sbin/shutdown"
#	"whenever system taken down"
#	"arg	added to /var/adm/wtmp to record reason, defaults to shutdown"
PATH=/usr/lib/acct:/bin:/usr/bin:/etc
_reason=${1-"acctg off"}
acctwtmp  "${_reason}"  >>/var/adm/wtmp
turnacct off
