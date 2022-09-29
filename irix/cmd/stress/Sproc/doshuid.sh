#!/bin/sh
# $Header: /proj/irix6.5.7m/isms/irix/cmd/stress/Sproc/RCS/doshuid.sh,v 1.2 1988/09/04 14:03:09 jwag Exp $

su <<!
cp shuid /usr/tmp/shuid
chown lp /usr/tmp/shuid
chgrp lp /usr/tmp/shuid
chmod 06755 /usr/tmp/shuid
exec su uucp -c "/usr/tmp/shuid $*"
!
