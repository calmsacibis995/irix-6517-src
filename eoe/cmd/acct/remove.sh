#!/sbin/sh
#	Copyright (c) 1993 UNIX System Laboratories, Inc.
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF
#	UNIX System Laboratories, Inc.   	
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#	copyright	"%c%"

#ident	"@(#)acct:common/cmd/acct/remove.sh	1.6.1.3"
#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/acct/RCS/remove.sh,v 1.3 1993/11/05 04:27:56 jwag Exp $"
rm -f /var/adm/acct/sum/wtmp*
rm -f /var/adm/acct/sum/pacct*
rm -f /var/adm/acct/nite/lock*
