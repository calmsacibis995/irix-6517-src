#! /sbin/sh

# Copyright 1995 Silicon Graphics, Inc.
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


# Start or stop the rtmon daemon 
# "$Revision: 1.7 $"
IS_ON=/etc/chkconfig
if $IS_ON verbose; then
    ECHO=echo
else            # For a quiet startup and shutdown
    ECHO=:
fi

CONFIG=/etc/config

RTMOND=/usr/etc/rtmond
OPTS=

case "$1" in
  'start')
      $ECHO "System event collection:\c"
      if $IS_ON rtmond; then
	  if test -x $RTMOND; then
	      if test -s $CONFIG/rtmond.options; then
		  OPTS="$OPTS `cat $CONFIG/rtmond.options`"
	      fi
	      /sbin/suattr -C CAP_SYSINFO_MGT,CAP_SCHED_MGT+ipe -c "$RTMOND $OPTS"
	      $ECHO " rtmond\c"
	  fi
      fi
      $ECHO "."
      ;;

  'stop')
      #
      # We're also named rtmond so blindly doing a killall
      # send us the signal too.  Consequently we ignore
      # SIGINT and use that since rtmond catches it and
      # properly cleans up it's state.
      #
      trap 2; /sbin/killall -INT rtmond
      ;;

  *)
      echo "usage: $0 {start|stop}"
      ;;
esac
