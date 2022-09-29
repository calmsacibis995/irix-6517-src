#!/sbin/sh
#	Copyright (c) 1993 UNIX System Laboratories, Inc.
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF
#	UNIX System Laboratories, Inc.   	
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#	copyright	"%c%"

#ident	"@(#)acct:common/cmd/acct/nulladm.sh	1.5.1.3"
#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/acct/RCS/nulladm.sh,v 1.2 1993/11/05 04:26:35 jwag Exp $"
#	"nulladm name..."
#	"creates each named file mode 664"
#	"make sure owned by adm (in case created by root)"
for _file
do
	test ! -f $_file && rm -f $_file
	test -f $_file -a ! -w $_file && rm -f $_file
	cp /dev/null $_file
	chmod 664 $_file
	chgrp adm $_file
	chown adm $_file
done
