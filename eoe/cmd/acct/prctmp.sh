#!/sbin/sh
#	Copyright (c) 1993 UNIX System Laboratories, Inc.
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF
#	UNIX System Laboratories, Inc.   	
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#	copyright	"%c%"

#ident	"@(#)acct:common/cmd/acct/prctmp.sh	1.7.1.3"
#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/acct/RCS/prctmp.sh,v 1.2 1993/11/05 04:26:47 jwag Exp $"
#	"print session record file (ctmp.h/ascii) with headings"
#	"prctmp file [heading]"
PATH=/usr/lib/acct:/bin:/usr/bin:/etc
(cat <<!; cat $*) | pr -h "SESSIONS, SORTED BY ENDING TIME"

MAJ/MIN			CONNECT SECONDS	START TIME	SESSION START
DEVICE	UID	LOGIN	PRIME	NPRIME	(NUMERIC)	DATE	TIME


!
