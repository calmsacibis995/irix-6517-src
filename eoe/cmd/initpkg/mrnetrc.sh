#! /bin/sh
#Tag 0x00000f00
#ident "$Revision $"

. mrprofrc

# mrnetrc {start|stop|netisup|forcestart [askipaddr]|sethostname|setipaddr
#	    |addhost hostname|validaddr ipaddr|sethostaddr hostname ipaddr
#	    |gethostaddr hostname}
#
#   Invoke with one of following:
#
#	start	    Copy up /root net config files and try to
#		    start network.  Don't fuss or interact if fails.
#
#	stop	    Invoke /etc/init.d/network stop.
#
#	netisup	    Return success iff network seems to be up.
#
#	forcestart  Set hostname, ip addr and run network start.
#		    Ok to ask user for hostname and/or ipaddr,
#		    if unable to discover automatically.
#
#		    Expects to be called from inst if earlier
#		    "mrnetrc start" failed to bring up networking,
#		    and user of inst tries to set "from" to a
#		    network address (has ':' in name).  Assumes earlier
#		    "mrnetrc start" copied up an available network config
#		    files from the normal /root file system.
#
#		    If optional 2nd arg "askipaddr" is present,
#		    then don't guess the ipaddr, ask instead.
#		    This "mrnetrc forcestart askipaddr" case is
#		    used to implement "mrnetrc setipaddr", below.
#
#	sethostname Ask user for this systems hostname and
#		    place in /etc/sys_id.  Then do forcestart.
#
#	setipaddr   Ask user for this hosts ipaddr and put
#		    into /etc/hosts (first removing any
#		    conflicting entries with same hostname).
#		    Then do forcestart.  Actually implemented
#		    by just calling "mrnetrc forcestart askipaddr".
#
#	addhost hostname
#		    If /etc/hosts doesn't already have an entry
#		    for specified hostname, then ask for ipaddr
#		    of that host, and append an entry for it in
#		    /etc/hosts.
#
#	validaddr ipaddr
#		    Exit 1 if the ip address is bogus (see below),
#		    otherwise exit 0.
#
#	sethostaddr hostname ipaddr
#		    Set the ip address of a host in /etc/hosts
#	gethostaddr hostname
#		    Get the ip address of a host in /etc/hosts,
#		    returns "0" if not found.

#
# Ignore SIGHUP.  This is needed so that processes that haven't had
# a change to run long enough to daemonize won't get killed when
# this script exits.
#
trap "" 1


#-------------- useful functions ---------------------

# comment_filter
#
# this sed script strips comment and empty lines.

comment_filter()
{
    sed -n -e '/#.*/s///' -e '/[ 	]*$/s///' -e /./p
			# beware ^^^^^^^ tab character
}

# gethostidbyname hostname
#
# Given a host or alias name that is in /etc/hosts, print the
# corresponding ip address of the first matching line on stdout.
#
# If hostname not found in /etc/hosts, print "0".  Comments beginning
# with '#' and ending with '\n' are suppressed.  Lines with < 2 fields
# (space or tab separated) are ignored.  This should match the way
# gethostbyname(3N) parses /etc/hosts.

gethostidbyname()
{
    test -s "/etc/hosts" || { echo 0; return; }

    comment_filter < /etc/hosts | awk '
            BEGIN { ipaddr = 0; }
            NF >= 2 {
                for (i = 2; i <= NF; i++) {
                    if ($i == "'"$1"'") {
                        ipaddr = $1
                        exit
                    }
                }
            }
	    END { print ipaddr }
	'

}

# gethostnamebyipaddr ipaddr
#
# Given an ip address, print the first corresponding hostname
# for it in /etc/hosts.  If not found, print empty string.

gethostnamebyipaddr()
{
    test -s "/etc/hosts" || return

    comment_filter < /etc/hosts | awk '
            NF >= 2 && $1 == "'"$1"'" {
		print $2
		exit
	    }
	'

}

# removehostsconflicts host
#
# Typically, only first /etc/hosts entry for a specified host
# is recognized.  Remove all lines matching on "$host", so that
# they do not conflict.  Call before appending entry to the
# end of /etc/hosts.
# Treats comment lines like any other:  if any field but the first
# matches "$host", remove the line.  Would be best if we treated
# comments special: passing them all through, unless they were a
# comment on a line that was being removed.  This however is a
# level of subtlety that is ill-defined, hard to do, and not worth it.

removehostsconflicts()
{
    test -f /etc/hosts || return
    awk < /etc/hosts > /etc/hosts.tmp '
        {   for (i = 2; i <= NF; i++) {
                if ($i == "'"$1"'")
                    next
            }
        }
        { print }
	'
    mv /etc/hosts.tmp /etc/hosts
}

# bogus ipaddr
#
# Decide if ip address bogus, such as 0, 0.0.0.0, 127.*.*.*
# (localhost) and 192.0.2.1 (test).
# Return 0 for success -- meaning _is_ bogus.
# Return 1 for failure -- meaning is not bogus.
#
# Set variable $reason with explanation, so that caller
# can if it chooses display the reason an address is bogus.

bogus()
{
    inet_aton "$1" || {
	reason='\nInvalid ascii Internet address representation: <'"$1"'>
Typical address is up to 4 . separated numbers.  Example: 192.0.2.2'
	return 0;
    }

    case "$1" in
	0|0x0|0.0|0.0.0|0.0.0.0)
	    reason='\nInternet address of zero <'"$1"'> not usually valid'
	    return 0
	    ;;
	127.*.*.*)
	    reason='\nInternet addresses 127.*.*.* reserved for loopback'
	    return 0
	    ;;
	192.0.2.1)
	    reason='\nInternet address <192.0.2.1> is for testing.  It loops back by default'
	    return 0
	    ;;
	"")
	    reason='\nPlease specify non-empty Internet address'
	    return 0;
	    ;;
	*)
	    return 1
	    ;;
    esac
}

# confirm message ...
#
# Display message.  Ask "Are you sure".
# Return with success if user answers yes.

confirm()
{
    echo "$*"
    echo '\n\tAre you sure? [y/n] (n): \c'
    read line
    case "$line" in
	[yY]*) return 0;;
	*) return 1;;
    esac
}

copynetfiles()
{
   (
	cp /root/etc/sys_id /etc/sys_id
	cp /root/etc/config/ifconfig-*.options /etc/config
	cp /root/usr/etc/resolv.conf /etc/resolv.conf   # old path
	cp /root/etc/resolv.conf /etc/resolv.conf       # new path
	cp /root/etc/config/netif.options /etc/config/netif.options
	chmod +x /etc/config/netif.options	# old inst does "test -x" !?!
	cp /root/etc/hosts /etc/hosts ||
	    echo "127.0.0.1 localhost loghost" > /etc/hosts
	chmod +x /etc/hosts			# old inst does "test -x" !?!
	chkconfig -f network on
    ) 2>/dev/null
}

savenetfiles()
{
   (
        mkdir /etc/config/.savedconfig 
        mv /etc/config/ifconfig-*.options /etc/config/.savedconfig
	mv /etc/config/netif.options /etc/config/.savedconfig
	mv /etc/sys_id /etc/.sys_id.saved
        cp /etc/hosts /etc/.hosts.saved
    ) 2>/dev/null
}

restorenetfiles()
{
   (
	rm /etc/config/ifconfig-*.options /etc/hosts /etc/sys_id
        mv /etc/config/.savedconfig/ifconfig-*.options /etc/config
	mv /etc/config/.savedconfig/netif.options /etc/config
        mv /etc/.hosts.saved /etc/hosts
	mv /etc/.sys_id.saved /etc/sys_id
	rm -rf /etc/config/.savedconfig
    ) 2>/dev/null
}


# $1 is the default (builtin) interface name
restartnetwork()
{
    chkconfig -f network on
    chkconfig verbose
    isverbose=$?

    # turn on verbose so we can inhibit that nasty graphical message
    chkconfig -f verbose on

    (
      /etc/init.d/network stop 
      ifconfig $1 down
      /etc/init.d/network start
    ) >/dev/null 2>&1

    if test $isverbose -ne 0 ; then
	chkconfig verbose off
    fi
}

defaultinterface()
{
    # this works even if there are interfaces without miniroot drivers
    hinv -c network | sed -ne 's/^Integr.* \([a-z][a-z]*0\),.*$/\1/p'
}

# return the number of interfaces (excluding lo0)
numinterfaces()
{
    ifconfig -a | awk '/^[a-z][a-z]*[0-9][0-9]*:/ {num += 1;} 
			/^lo0:/ {num -= 1;}
			END {print num;}'
}

ishex()
{
    echo "$1" | grep -i "^0x[0-9,a-f][0-9,a-f][0-9,a-f][0-9,a-f][0-9,a-f][0-9,a-f][0-9,a-f][0-9,a-f]$" > /dev/null

}

# $1 is the interface name
# $2 is the optional pathname prefix (probably /root)
getinterfacenum()
{
    test $# -eq 0 && return
  
    interfacefile="/etc/config/netif.options"
    if test $# -gt 1 ; then
	interfacefile=$2$interfacefile
    fi

  (
    # mimic what happens in /etc/init.d/network script so we can
    # accurately predict what ifconfig options file will have the 
    # integral interface information
    if_num=9
    if ( ifconfig ipg0  || ifconfig xpi0 || ifconfig rns0 ) >/dev/null 2>&1
    then
	if2name="$1"
	num=2
    else
	if1name="$1"
	num=1
    fi

    test -s "$interfacefile" || { echo -n $num; return; }

    . "$interfacefile"

    num=1
    while test $num -le $if_num; do
	eval _ifname='$'if${num}name 
	if test "$_ifname" = "$1" ; then
	    echo -n $num
	    return
	fi
	num=`expr $num + 1`
    done
  )

}

# $1 is ip address
getclassdefaultnetmask()
{
    test $# -eq 1 || return

    echo "$1" | nawk -F\. '{
		if ($1 < 128) {print "0xff000000"}
		else if ($1 < 192) {print "0xffff0000"}
		else if ($1 < 224) {print "0xffffff00"}
		else {print "0xffffffff"}
	}'
}


# $1 is optional path prefix (ie. /root)

getnetmask()
{
    nvramnetmask=`nvram netmask 2>/dev/null`
    if test $nvramnetmask && \
       ( ishex "$nvramnetmask" || ! bogus "$nvramnetmask" ) ; then
	echo -n "$nvramnetmask"
	return
    fi

    interface=`defaultinterface`

    prefix=""
    if test $# -gt 0 ; then
	prefix="$1"
    fi

    num=`getinterfacenum "$interface" "$prefix"`

    test "$num" || exit 1

    ethnetmask=0
    ifconfigname=$prefix/etc/config/ifconfig-"$num".options

    if test -s "$ifconfigname" ; then
	configoptions="`cat $ifconfigname 2> /dev/null`"
	set $configoptions
	while test $# -gt 1 ; do
	    if test "$1" = "netmask" ; then
		ethnetmask=$2
		break
	    fi
	    shift
	done
    fi
    # debugging code was useful
    # echo 1>&2 mrnetrc getnetmask: ethnetmask= "\"$ethnetmask\""

    if ishex "$ethnetmask" || ! bogus "$ethnetmask" ; then
	echo -n "$ethnetmask"
    fi
}

# $1 is hostname
# $2 is ip address
# $3 is interface name
# $4 is netmask
buildnetworkfiles()
{
    test $# -eq 4 || return 1

    savenetfiles
    echo "$1" > /etc/sys_id
    hostname "$1"

    echo "if1name=$3\nif_num=1\nif1addr=$1\n" > /etc/config/netif.options

    if test -n "$4" ; then
	echo netmask "$4" > /etc/config/ifconfig-1.options 
    fi

    removehostsconflicts "$1"
    echo "$2 $1" >> /etc/hosts
}

# converts $1 IP address (or netmask) argument to hex format
tohex()
{
    echo $1 | awk -F. '
	/^0x/ && NF == 1 {a=substr($1,3);printf "%s\n",a; exit 0}
	NF == 2 {printf "%02x%06x\n",$1,$2; exit 0}
	NF == 3 {printf "%02x%02x%04x\n",$1,$2,$3; exit 0}
	NF == 4 {printf "%02x%02x%02x%02x\n",$1,$2,$3,$4; exit 0}
	{exit 1}
    '
}

# $1 is the hostname ( if empty minimal networking is started using ifconfig )
# $2 is the root path where to look for network config files (ie /root or "")
# $2 is used to determine the netmask in the event nvram netmask is not set
setupbuiltinnetworking()
{
    test $# -eq 2 || return 1

    etheripaddr=`nvram netaddr 2>/dev/null || echo 0`
    bogus "$etheripaddr" && return 1

    interface=`defaultinterface`
    test $interface || return 1

    ethernetmask=`getnetmask "$2"`

    # debugging code
    # echo "mrnetrc: setupbuiltinnetworking \"$interface\" \"$etheripaddr\" \"$ethernetmask\""

    # if no hostname provided we should configure networking manually and 
    # bypass the network script entirely since it will put us in standalone
    if test -z "$1" ; then
        killall routed
        if test "$ethernetmask" ; then
	    ifconfig "$interface" inet "$etheripaddr" primary netmask "$ethernetmask"
	else
	    ifconfig "$interface" inet "$etheripaddr" primary 
	fi
	hostid 0x`tohex $etheripaddr`
	if chkconfig routed ; then
	    routed_opts="`cat /etc/config/routed.options 2> /dev/null`"
	    routed "$routed_opts"
	fi
        if mrinterface ; then
	    if test -s /etc/sys_id ; then
		hostname `sed -n p /etc/sys_id`
		removehostsconflicts `hostname`
		echo "$etheripaddr	`hostname`" >> /etc/hosts
	    fi
	    return 0
	fi
	return 1
    fi

    # otherwise, save off all the old networking files and create
    # new ones with the new derived values and then restart networking

    buildnetworkfiles $1 $etheripaddr $interface $ethernetmask

    restartnetwork $interface
    hostid 0x`tohex $etheripaddr`

    removehostsconflicts $1
    echo "$etheripaddr	$1" >> /etc/hosts

    if ! mrinterface ; then
	restorenetfiles
	if test -f /etc/sys_id ; then
	    hostname `sed -n p /etc/sys_id`
	fi
	return 1
    fi

    return 0
}


#-------------- main procedures -----------------------

case "$1" in

start)

    # This is a no-op in custom mode, since minimal networking has
    # already started if we got this far.
    if mrcustomrc iscustom ; then
	exit 0
    fi

    # Best effort network start.  Copy up user's network
    # configuration from /root, and run "network start"
    # Don't fuss or interact if unable to do some or all of this.
    # If later on, we decide we really must have networking on,
    # then call "mrnetrc forcestart" at that time.

    copynetfiles

    if test -f /etc/sys_id -a -f /root/etc/hosts
    then

	# Use "sed -n p" instead of "cat" because cat command
	# can fail, writing to a fifo, when the next fifo inode
	# number assigned by the kernel happens to match the
	# inode number of the file being cat'd.  The failure
	# complains: Input/output files '/etc/sys_id' identical

	hostname `sed -n p /etc/sys_id`

	/etc/init.d/network start
    fi
    ;;

stop)

    /etc/init.d/network stop
    ;;

netisup)

    # If network already started successfully, return success (0) else failure.
    #
    # Use mrinterface to check for the network interfaces that are UP.
    # mrinterface will return 3 possible values : -1 = error
    # 			0 = success (network UP - found network interface)
    # 			1 = failure
    #

    mrinterface
    ;;

forcestart)
    
    # Note the NVRAM's netaddr.  We might use this to guess
    # the desired hostname, if sys_id doesn't help.  And
    # later we might use it to determine this hosts ip address,
    # if the address (if any) listed for this host in /etc/hosts
    # is bogus.
    
    nvramipaddr=`nvram netaddr 2>/dev/null || echo 0`
    
    # Determine hostname from /etc/sys_id, or by looking up
    # the NVRAM's netaddr in the /etc/hosts file, or from
    # asking user.  Then put it in /etc/sys_id, if need be.
    # See explanation above for use of "sed -n p" instead of "cat".
    
    test -s /etc/sys_id && host=`sed -n p /etc/sys_id` || {
	# Try to find a hostname by looking up nvram's netaddr
	# in /etc/hosts.
    
	host=`gethostnamebyipaddr "$nvramipaddr"`
    
	# If that came up empty, have to ask the user.
	# We accept any non-zero length string.
    
	while test -z "$host"
	do
	    echo 'What is the hostname (system name) of your machine? \c'
	    read host
	    if [ x"$host" = x"!" ]; then
	        echo "Type exit when ready to continue"
	        /bin/csh
		host=
	    fi
	done
	echo "$host" > /etc/sys_id
    }
    
    # Set the system hostname
    
    hostname "$host"
    
    defaultint=`defaultinterface`
    defaultnetmask=`getnetmask`
    askuser=0
    if test $# -eq 2 -a askipaddr = "$2" || \
       test -z "$defaultint" || bogus "$nvramipaddr" ; then
       askuser=1
    fi


    # While still bogus, ask user if ok.
    # If user confirms, quit asking.
    
    if test "$askuser" -eq 1 ; then

	if test `numinterfaces` -eq 1 ; then
	    interface=$defaultint
	else
	    interface=
	fi
	while ! ifconfig "$interface" >/dev/null 2>&1; do
	    if test -n "$interface" ; then
		confirm Using network interface "$interface".'  \c' && break
	    fi
	    if test -z "$defaultint" ; then
		echo What network interface would you like to configure? '\c'
	    else
		echo What network interface would you like to configure?
		echo Press Enter for builtin interface [$defaultint] '\c'
	    fi
	    read interface
	    if test -z "$interface" ; then
		interface=$defaultint
	    fi
	    if [ x"$interface" = x"!" ]; then
	        echo "Type exit when ready to continue"
	        /bin/csh
		interface=
	    fi
	done

	ipaddr=
	while bogus "$ipaddr" ; do
	    if test -n "$ipaddr" ; then
		echo "$reason"
		confirm Using network address: "$ipaddr" for: "$host".'  \c' && break
	    fi
	    echo What is the network address of "$host"? '\c'
	    read ipaddr
	    if [ x"$ipaddr" = x"!" ]; then
	        echo "Type exit when ready to continue"
	        /bin/csh
		ipaddr=
	    fi
	done
    else
	interface=$defaultint
	ipaddr=$nvramipaddr
    fi

    if test -z "$defaultnetmask" -o "$nvramipaddr" != "$ipaddr" -o \
       "$interface" != "$defaultint" ; then
       netmask=
       while ! ishex "$netmask" && bogus "$netmask" ; do
	   if test -n "$netmask" ; then
		echo "$netmask" is an invalid netmask.
	   fi
	   defaultnetmask=`getclassdefaultnetmask "$ipaddr"`
	   echo What is the netmask for "$ipaddr"?
	   echo "Press Enter for the IP class default [$defaultnetmask]: \c"
	   read netmask
	   if test -z "$netmask" ; then
		netmask="$defaultnetmask"
		break
	   fi
	   if [ x"$netmask" = x"!" ]; then
	       echo "Type exit when ready to continue"
	       /bin/csh
	       netmask=
	   fi
       done
    else
	netmask=$defaultnetmask
    fi

    buildnetworkfiles "$host" "$ipaddr" "$interface" "$netmask"
    echo Starting network with hostname: "$host", at ip address: "$ipaddr"
    
    restartnetwork "$interface"
    
    mrinterface

    ;;
  
sethostname)
	# Ask the user for the hostname to set.
	# We accept any non-zero length string.

	host=
	while test -z "$host"
	do
	    echo 'What is the hostname (system name) of your machine? \c'
	    read host
	    if [ x"$host" = x"!" ]; then
		echo "Type exit when ready to continue"
		/bin/csh
		host=
	    fi
	done
	echo "$host" > /etc/sys_id

	# restart the network

	exec $0 forcestart
	echo Failed to restart the network 1>&2
    ;;

setipaddr)
	exec $0 forcestart askipaddr
	echo Failed to invoke $0 forcestart askipaddr 1>&2
    ;;

validaddr)
	bogus "$2" && exit 1
	exit 0
    ;;

sethostaddr)
	bogus "$3" && exit 1
	bogus "$2" || exit 1
	removehostsconflicts "$2"
	echo "$3 $2" >> /etc/hosts
    ;;

gethostaddr)
	bogus "$2" || exit 1
	gethostidbyname "$2"
    ;;

addhost)

    # put entry for specified host (2nd arg) in /etc/hosts file.

    test $# -eq 2 || exit
    host=$2

    # is this host already in hosts -- if so, nothing to do, just leave

    hostipaddr=`gethostidbyname "$host"`
    test $hostipaddr != 0 && exit 0

    # if asked to add host by name that looks like an ipaddr,
    # then do nothing - user provided host ipaddr instead of hostname,
    # so don't need to translate name to ipaddr -- already is.

    bogus "$host" || exit 0

    # if this is the same host we booted from, get the address from bootp

    tapedevice=`nvram tapedevice 2>/dev/null || echo 0`
    boothost=`echo "$tapedevice" | sed -n 's;.*bootp()\([^ :]*\).*;\1;p'`
    bootfile=`echo "$tapedevice" | sed -n 's;.*bootp()\([^ ()]*\).*;-b\1;p'`
    if [ "$boothost" = "$host" ]; then
        /usr/etc/bootpc -r $bootfile -x4 > /tmp/bootp$$ 2>/dev/null
	if [ -s /tmp/bootp$$ ]; then
	    ipaddr=`sed -n 's/^bootpaddr=//p' /tmp/bootp$$`
	fi
	rm -f /tmp/bootp$$ > /dev/null 2>&1
	bogus "$ipaddr" || {
	    echo "$ipaddr $host" >> /etc/hosts
	    exit 0
	}
    fi

    # ask user for ipaddr of host

    while : asking
    do
	echo What is the network address of "$host"? '\c'
	read ipaddr
	if [ x"$ipaddr" = x"!" ]; then
	    echo "Type exit when ready to continue"
	    /bin/csh
	else
	    break;
	fi
    done
   
    # While still bogus, ask user if ok.
    # If user confirms, quit asking.
    
    while bogus "$ipaddr"
    do
	echo "$reason"
	confirm Using network address: "$ipaddr" for: "$host".'  \c' && break
	echo What is the network address of "$host"? '\c'
	read ipaddr
	if [ x"$ipaddr" = x"!" ]; then
	    echo "Type exit when ready to continue"
	    /bin/csh
	    ipaddr=
	fi
    done
    
    # Now we can append our line to /etc/hosts
    
    echo "$ipaddr $host" >> /etc/hosts

    ;;

safestart)

    # basically like "start", but traps the output and if any errors
    # occur, will shutdown and do a "setupbuiltinnetworking"

    defaultint=`defaultinterface`
    if test -s /root/etc/config/netif.options -a -s /root/etc/sys_id -a -s /root/etc/hosts ; then
	# there's a chance that all the needed configuration files are valid
    	copynetfiles
	hostname=`sed -n p /etc/sys_id`
	hostname $hostname
	restartnetwork "$defaultint"
    fi

    if ! mrinterface ; then
	ifconfig "$defaultint" down
	setupbuiltinnetworking "$hostname" /root
    fi
    mrinterface
    ;;

startdefaultinterface)

    # the hostname and rbase of tree to look for network files (/root or "")
    # if an empty hostname is provided, networking is configured manually 
    # using ifconfig rather than calling the network scripts

    test $# -eq 3 || exit 1

    setupbuiltinnetworking "$2" "$3"

    ;;
*)
    echo "Usage: $0 {start|stop|netisup|forcestart [askipaddr]|sethostname|setipaddr|addhost hostname|validaddr ipaddr|sethostaddr host ipaddr|startdefaultinterface host rbase }"
    ;;

esac
