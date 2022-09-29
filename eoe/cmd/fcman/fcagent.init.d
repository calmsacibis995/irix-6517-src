#!/bin/sh
#
# Start and stop fcagent(1M)
#
# Copyright 1994 Silicon Graphics, Inc.
# All rights reserved.
#
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
# the contents of this file may not be disclosed to third parties, copied or
# duplicated in any form, in whole or in part, without the prior written
# permission of Silicon Graphics, Inc.
#
# RESTRICTED RIGHTS LEGEND:
# Use, duplication or disclosure by the Government is subject to restrictions
# as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
# and Computer Software clause at DFARS 252.227-7013, and/or in similar or
# successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
# rights reserved under the Copyright Laws of the United States.
#
# $Revision: 1.3 $

IS_ON=/etc/chkconfig
FCAGENT=/usr/sbin/fcagent
FCAGENT_CFG=/etc/config/fcagent.options

if $IS_ON verbose ; then
    ECHO=echo
    VERBOSE=-v
else            # For a quiet startup and shutdown
    ECHO=:
    VERBOSE=
fi

case "$1" in
'start')
	if $IS_ON fcagent && test -x $FCAGENT && test -f $FCAGENT_CFG; then
		$ECHO "Fibre Channel daemons: fcagent\c"
		$FCAGENT $FCAGENT_CFG 2> /dev/null &
		$ECHO "."
	fi
	;;

'stop')
	if test -x $FCAGENT; then
		killall fcagent
	fi
	;;

*)
	echo "usage: $0 {start|stop}"
	;;
esac
