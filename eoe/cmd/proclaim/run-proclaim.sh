#!/sbin/sh

# Script to be used with the DHCP client program for
#
# start - getting an IP address and other configuration info
# stop  - to kill a running client
# verify - to verify existing lease (will also extend it)
# surrender - to surrender the current lease

# $Revision: 1.4 $

CONFIG=/etc/config
IS_ON=/sbin/chkconfig
PROCLAIM=/usr/etc/proclaim
KILLALL=/sbin/killall
IFCONFIG=/usr/etc/ifconfig
GREP=/sbin/grep

if $IS_ON autoconfig_ipaddress; then
    :
else
    exit 0
fi

if $IS_ON verbose ; then
    ECHO=echo
    VERBOSE=-v
else		# For a quiet startup and shutdown
    ECHO=:
    VERBOSE=
fi
	

case "$1" in
'start')
	$ECHO "IP Address Configuration: "
	$KILLALL proclaim
	# borrowed partly from the network script
	# If FDDI is present, make it is the primary interface
	if $IFCONFIG ipg0 >/dev/null 2>&1; then
		if1name=ipg0
	elif $IFCONFIG xpi0 >/dev/null 2>&1; then
		if1name=xpi0
	elif $IFCONFIG rns0 >/dev/null 2>&1; then
		if1name=rns0
	fi

	if test "$if1name"; then
		:
	elif $IFCONFIG et0 >/dev/null 2>&1; then
		if1name=et0
	elif $IFCONFIG ec0 >/dev/null 2>&1; then
		if1name=ec0
	elif $IFCONFIG fxp0 >/dev/null 2>&1; then
		if1name=fxp0
	elif $IFCONFIG enp0 >/dev/null 2>&1; then
		if1name=enp0
	fi

	# Obtain site-dependent values for if1name
	if test -s $CONFIG/netif.options; then
		. $CONFIG/netif.options
	fi

	# start proclaim on primary if needed
	primary=on
	if grep "Primary" $CONFIG/proclaim.options >/dev/null 2>&1; then
		if grep "Primary" $CONFIG/proclaim.options | grep "off" >/dev/null 2>&1 ; then
			primary=off
		fi
	fi

	if test $primary = on ; then
		$ECHO "	Starting DHCP client on Primary interface $if1name"
		$PROCLAIM -i
		if [ -x /usr/etc/dhcpcopt ]; then
		    /usr/etc/dhcpcopt $if1name
		fi
	fi

	# start proclaim on other interfaces
	grep -v "#" $CONFIG/proclaim.options | grep "Interface" |
	while read iface ifname onoff
	do
		if [ $onoff = on -a $if1name != $ifname ]  ; then
			$PROCLAIM -i -e $ifname
			$ECHO "	Starting DHCP client on Interface $ifname"
		fi  
		if [ -x /usr/etc/dhcpcopt ]; then
		    /usr/etc/dhcpcopt $ifname
		fi
	done


	$ECHO "done."
	;;

'stop')
	$KILLALL proclaim
	;;

'surrender')
	if $KILLALL -USR1 proclaim; then
		:
	else
		$ECHO "Try: $PROCLAIM -d -s server_addr"
	fi
	;;

'verify')
	if $KILLALL -USR2 proclaim; then
		:
	else
		$PROCLAIM -i
	fi
	;;
'status')
	$PROCLAIM -p
	;;

*)
    $ECHO "usage: $0 {start|stop|surrender|verify|status}"
    exit 1
    ;;
esac

