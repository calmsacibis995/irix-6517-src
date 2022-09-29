#!/bin/sh

IS_ON=/etc/chkconfig
PROCLAIM=/usr/etc/proclaim

if $IS_ON autoconfig_ipaddress && test -x $PROCLAIM; then
	$PROCLAIM
	if [ $? -eq 0 ]; then
    		$IS_ON autoconfig_ipaddress off
	fi
fi
