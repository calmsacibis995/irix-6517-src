#!/sbin/sh
#	Copyright (c) 1993 UNIX System Laboratories, Inc.
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF
#	UNIX System Laboratories, Inc.   	
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#	copyright	"%c%"

#ident	"@(#)acct:common/cmd/acct/prtacct.sh	1.9.1.3"
#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/acct/RCS/prtacct.sh,v 1.2 1993/11/05 04:27:10 jwag Exp $"
#	"print daily/summary total accounting (any file in tacct.h format)"
#	"prtacct file [heading]"
PATH=/usr/lib/acct:/bin:/usr/bin:/etc
_filename=${1?"missing filename"}
(cat <<!; acctmerg -t -a <${_filename}; acctmerg -p <${_filename}) | pr -h "$2"
	LOGIN 	   CPU (MINS)	  KCORE-MINS	CONNECT (MINS)	DISK	# OF	# OF	# DISK	FEE
UID	NAME 	 PRIME	NPRIME	PRIME	NPRIME	PRIME	NPRIME	BLOCKS	PROCS	SESS	SAMPLES	
!
