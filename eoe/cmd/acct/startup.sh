#!/sbin/sh
#	Copyright (c) 1993 UNIX System Laboratories, Inc.
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF
#	UNIX System Laboratories, Inc.   	
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#	copyright	"%c%"

#ident	"@(#)acct:common/cmd/acct/startup.sh	1.7.1.4"
#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/acct/RCS/startup.sh,v 1.3 1993/11/05 04:28:19 jwag Exp $"
#	"startup (acct) - should be called from /etc/rc"
#	"whenever system is brought up"
PATH=/usr/lib/acct:/bin:/usr/bin:/etc
acctwtmp "acctg on" >>/var/adm/wtmp
turnacct on
#	"clean up yesterdays accounting files"
rm -f /var/adm/acct/sum/wtmp*
rm -f /var/adm/acct/sum/pacct*
rm -f /var/adm/acct/nite/lock*
