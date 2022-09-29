#!/bin/sh
#
# pmlogconf - generate/edit a pmlogger configuration file
#
# control lines have this format
# #+ tag:on-off:delta
# where
#	tag	matches [A-Z][0-9a-z] and is unique
#	on-off	y or n to enable or disable this group
#	delta	delta argument for pmlogger "logging ... on delta" clause
#	
# $Id: pmlogconf.sh,v 1.2 1999/05/27 18:55:08 kenmcd Exp $
#
#
# Copyright 1998, Silicon Graphics, Inc.
# ALL RIGHTS RESERVED
# 
# UNPUBLISHED -- Rights reserved under the copyright laws of the United
# States.   Use of a copyright notice is precautionary only and does not
# imply publication or disclosure.
# 
# U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
# Use, duplication or disclosure by the Government is subject to restrictions
# as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
# in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
# in similar or successor clauses in the FAR, or the DOD or NASA FAR
# Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
# 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
# 
# THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
# INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
# DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
# PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
# GRAPHICS, INC.

prog=`basename $0`

if [ $# -ne 1 -o "$1" = "-?" ]
then
    echo "Usage: $prog configfile"
    exit 1
fi

tmp=/var/tmp/$$
trap "rm -f $tmp.*; exit" 0 1 2 3 15
rm -f $tmp.*
prompt=true

# split $tmp.ctl at the line containing the unprocessed tag
#
_split()
{
    rm -f $tmp.head $tmp.tag $tmp.tail
    nawk <$tmp.ctl '
BEGIN						{ out = "'"$tmp.head"'" }
seen == 0 && /^#\? [A-Z][0-9a-z]:[yn]:/		{ print >"'"$tmp.tag"'"
						  out = "'"$tmp.tail"'"
						  seen = 1
						  next
						}
						{ print >out }'
}

if [ ! -f $1 ]
then
    touch $1
    if [ ! -f $1 ]
    then
	echo "$prog: Error: config file \"$1\" does not exist and cannot be created"
	exit 1
    fi

    echo "Creating config file \"$1\" using default settings \c"
    prompt=false

    # see below for tag explanations
    #
    touch $1
    cat <<End-of-File >>$tmp.in
#$prog 1.0
# $prog control file version
#
# pmlogger(1) config file created and updated by
# $prog(1).
#
# DO NOT UPDATE THE INTITIAL SECTION OF THIS FILE.
# Any changes may be lost the next time $prog is used
# on this file.
#

# System configuration
#
#+ I0:y:once:
#----

# Disk activity
#
#+ D0:y:default:
#----
#+ D1:n:default:
#----
#+ D2:n:default:
#----
#+ D3:n:default:
#----

# CPU activity
#
#+ C0:y:default:
#----
#+ C1:n:default:
#----

# Kernel activity
#
#+ K0:y:default:
#----
#+ K1:y:default:
#----
#+ K2:n:default:
#----
#+ K3:n:default:
#----
#+ K4:n:default:
#----
#+ K5:n:default:
#----
#+ K6:n:default:
#----
#+ K7:n:default:
#----
#+ K8:n:default:
#----
#+ K9:n:default:
#----

# Memory
#
#+ M0:y:default:
#----
#+ M1:n:default:
#----
#+ M2:n:default:
#----
#+ M3:n:default:
#----
#+ M4:n:default:
#----
#+ M5:n:default:
#----
#+ M6:n:default:
#----

# Network
#
#+ N0:y:default:
#----
#+ N1:n:default:
#----
#+ N2:n:default:
#----
#+ N3:n:default:
#----
#+ N4:n:default:
#----
#+ N5:n:default:
#----
#+ N6:n:default:
#----
#+ N7:n:default:
#----
#+ N8:n:default:
#----
#+ N9:n:default:
#----
#+ Na:n:default:
#----

# Services
#
#+ S0:n:default:
#----
#+ S1:n:default:
#----
#+ S2:n:default:
#----

# Filesystems and Volumes
#
#+ F0:n:default:
#----
#+ F1:y:default:
#----
#+ F2:n:default:
#----
#+ F3:n:default:
#----
#+ F4:n:default:
#----
#+ F5:n:default:
#----

# Hardware event counters
#
#+ H0:n:default:
#----
#+ H1:n:default:
#----
#+ H2:n:default:
#----
#+ H3:n:default:
#----

# DO NOT UPDATE THE FILE ABOVE THIS LINE
# Otherwise any changes may be lost the next time $prog is
# used on this file.
#
# It is safe to make additions from here on ...
#

End-of-File

else
    magic=`sed 1q $1`
    if echo "$magic" | grep "^#$prog" >/dev/null
    then
	version=`echo $magic | sed -e "s/^#$prog//" -e 's/^  *//'`
	if [ "$version" = "1.0" ]
	then
	    :
	else
	    echo "$prog: Error: existing config file \"$1\" is wrong version ($version)"
	    exit 1
	fi
    else
	echo "$prog: Error: existing \"$1\" is not a $prog control file"
	exit 1
    fi
    if [ ! -w $1 ]
    then
	echo "$prog: Error: existing config file \"$1\" is not writeable"
	exit 1
    fi
    cp $1 $tmp.in
fi

# strip the existing pmlogger config and leave the comments
# and the control lines
#

nawk <$tmp.in '
/DO NOT UPDATE THE FILE ABOVE/	{ tail = 1 }
tail == 1			{ print; next }
/^\#\+ [A-Z][0-9a-z]:[yn]:/	{ print; skip = 1; next }
skip == 1 && /^\#----/		{ skip = 0; next }
skip == 1			{ next }
				{ print }' \
| sed -e '/^#+ [A-Z][0-9a-z]:[yn]:/s/+/?/' >$tmp.ctl

while true
do
    _split
    [ ! -s $tmp.tag ] && break
    eval `sed <$tmp.tag -e 's/^#? /tag="/' -e 's/:/" onoff="/' -e 's/:/" delta="/' -e 's/:.*/"/'`
    [ -z "$delta" ] && delta=default

    case $onoff
    in
	y|n)	;;
	*)	echo "Warning: tag=$tag onoff is illegal ($onoff) ... setting to \"n\""
		onoff=n
		;;
    esac

    desc="Unknown group, tag \"$tag\""
    metrics=""
    metrics_a=""
    case $tag
    in
	I0)	desc="hardware configuration"
		metrics="hinv"
		;;

	D0)	desc="activity (IOPs and bytes for both reads and writes) over all disks"
		metrics="disk.all.read
			disk.all.write
			disk.all.total
			disk.all.read_bytes
			disk.all.write_bytes
			disk.all.bytes"
		;;

	D1)	desc="per controller disk activity"
		metrics="disk.ctl.read
			disk.ctl.write
			disk.ctl.total
			disk.ctl.read_bytes
			disk.ctl.write_bytes
			disk.ctl.bytes"
		;;

	D2)	desc="per spindle disk activity"
		metrics="disk.dev.read
			disk.dev.write
			disk.dev.total
			disk.dev.read_bytes
			disk.dev.write_bytes
			disk.dev.bytes"
		;;

	D3)	desc="all available data per disk spindle"
		metrics="disk.dev"
		;;

	C0)	desc="utilization (usr, sys, idle, ...) over all CPUs"
		metrics="kernel.all.cpu.idle
			kernel.all.cpu.intr
			kernel.all.cpu.sys
			kernel.all.cpu.sxbrk
			kernel.all.cpu.user
			kernel.all.cpu.wait.total"
		;;

	C1)	desc="utilization per CPU"
		metrics="kernel.percpu.cpu.idle
			kernel.percpu.cpu.intr
			kernel.percpu.cpu.sys
			kernel.percpu.cpu.sxbrk
			kernel.percpu.cpu.user
			kernel.percpu.cpu.wait.total"
		;;

	K0)	desc="load average and number of logins"
		metrics="kernel.all.load
			kernel.all.users"

		;;

	K1)	desc="context switches, total syscalls and counts for selected calls (e.g. read, write, fork, exec, select) over all CPUs"
		metrics="kernel.all.pswitch
			kernel.all.syscall
			kernel.all.sysexec
			kernel.all.sysfork
			kernel.all.sysread
			kernel.all.syswrite"
		metrics_a="kernel.all.kswitch
			kernel.all.kpreempt
			kernel.all.sysioctl"
		;;

	K2)	desc="per CPU context switches, total syscalls and counts for selected calls"
		metrics="kernel.percpu.pswitch
			kernel.percpu.syscall
			kernel.percpu.sysexec
			kernel.percpu.sysfork
			kernel.percpu.sysread
			kernel.percpu.syswrite"
		metrics_a="kernel.percpu.kswitch
			kernel.percpu.kpreempt
			kernel.percpu.sysioctl"
		;;

	K3)	desc="bytes across the read() and write() syscall interfaces"
		metrics="kernel.all.readch kernel.all.writech"
		;;

	K4)	desc="interrupts"
		metrics="kernel.all.intr.vme
			kernel.all.intr.non_vme
			kernel.all.tty.recvintr
			kernel.all.tty.xmitintr
			kernel.all.tty.mdmintr"
		;;

	K5)	desc="buffer cache reads, writes, hits and misses"
		metrics="kernel.all.io.bread
			kernel.all.io.bwrite
			kernel.all.io.lread
			kernel.all.io.lwrite
			kernel.all.io.phread
			kernel.all.io.phwrite
			kernel.all.io.wcancel"
		;;

	K6)	desc="all available buffer cache data"
		metrics="buffer_cache"
		;;

	K7)	desc="vnode activity"
		metrics="vnodes"
		;;

	K8)	desc="name cache (namei, iget, etc) activity"
		metrics="kernel.all.io.iget
			kernel.all.io.namei
			kernel.all.io.dirblk
			name_cache"
		;;

	K9)	desc="asynchronous I/O activity"
		metrics="kaio"
		;;

	M0)	desc="page outs (severe VM demand)"
		metrics="swap.pagesout"
		;;

	M1)	desc="address translation (faults and TLB activity)"
		metrics="mem.fault mem.tlb"
		;;

	M2)	desc="kernel memory allocation"
		metrics="mem.system
			mem.util
			mem.freemem
			mem.availsmem
			mem.availrmem
			mem.bufmem
			mem.physmem
			mem.dchunkpages
			mem.pmapmem
			mem.strmem
			mem.chunkpages
			mem.dpages
			mem.emptymem
			mem.freeswap
			mem.halloc
			mem.heapmem
			mem.hfree
			mem.hovhd
			mem.hunused
			mem.zfree
			mem.zonemem
			mem.zreq
			mem.iclean
			mem.bsdnet
			mem.palloc
			mem.unmodfl
			mem.unmodsw
			mem.paging.reclaim"
		;;

	M3)	desc="current swap allocation and all swap activity"
		metrics="swap"
		;;

	M4)	desc="swap configuration"
		metrics="swapdev"
		;;

	M5)	desc=""large" page and O2000 node-based allocations and activity"
		metrics="mem.lpage"
		metrics_a="origin.node"
		;;

	M6)	desc="NUMA stats"
		metrics="origin.numa"
		;;

	N0)	desc="bytes and packets (in and out) and bandwidth per interface"
		metrics="network.interface.in.bytes
			network.interface.in.packets
			network.interface.out.bytes
			network.interface.out.packets
			network.interface.total.bytes
			network.interface.total.packets
			network.interface.baudrate"
		;;

	N1)	desc="all available data per interface"
		metrics="network.interface"
		;;

	N2)	desc="TCP bytes and packets (in and out), connects, accepts, drops and closes"
		metrics="network.tcp.accepts
			network.tcp.connattempt
			network.tcp.connects
			network.tcp.drops
			network.tcp.conndrops
			network.tcp.closed
			network.tcp.sndtotal
			network.tcp.sndpack
			network.tcp.sndbyte
			network.tcp.rcvtotal
			network.tcp.rcvpack
			network.tcp.rcvbyte"
		;;

	N3)	desc="all available TCP data"
		metrics="network.tcp"
		;;

	N4)	desc="UDP packets in and out"
		metrics="network.udp.ipackets network.udp.opackets"
		;;

	N5)	desc="all available UDP data"
		metrics="network.udp"
		;;

	N6)	desc="socket stats (counts by type and state)"
		metrics="network.socket"
		;;

	N7)	desc="all available data for other protocols (IP, ICMP, IGMP)"
		metrics="network.ip
			network.icmp
			network.igmp"
		;;

	N8)	desc="mbuf stats (alloc, failed, waited, etc)"
		metrics="network.mbuf"
		;;

	N9)	desc="multicast routing stats"
		metrics="network.mcr"
		;;

	Na)	desc="SVR5 streams activity"
		metrics="resource.nstream_queue
			resource.nstream_head"
		;;

	S0)	desc="NFS2 stats"
		metrics="nfs"
		;;

	S1)	desc="NFS3 stats"
		metrics="nfs3"
		;;

	S2)	desc="RPC stats"
		metrics="rpc"
		;;

	F0)	desc="Filesystem fullness"
		metrics="filesys"
		;;

	F1)	desc="XFS data and log traffic"
		metrics="xfs.log_writes
			xfs.log_blocks
			xfs.log_noiclogs
			xfs.read_bytes
			xfs.write_bytes"
		;;

	F2)	desc="all available XFS data"
		metrics="xfs"
		;;

	F3)	desc="XLV operations and bytes per volume"
		metrics="xlv.read
			xlv.write
			xlv.read_bytes
			xlv.write_bytes"
		;;

	F4)	desc="XLV striped volume stats"
		metrics="xlv.stripe_ops
			xlv.stripe_units
			xlv.aligned
			xlv.unaligned
			xlv.largest_io"
		;;

	F5)	desc="EFS activity"
		metrics="efs"
		;;

	H0)	desc="CrayLink routers"
		metrics="hw.router"
		;;

	H1)	desc="O2000 hubs"
		metrics="hw.hub"
		;;

	H2)	desc="global R10000 event counters (enable first with ecadmin(1))"
		metrics="hw.r10kevctr"
		;;

	H3)	desc="XBOW activity"
		metrics="xbow"
		;;

    esac

    if $prompt
    then
    	# prompt for answers
	#
	echo
	echo "Group: $desc" | fmt -78 | sed -e '1!s/^/       /'
	while true
	do
	    echo "Log this group? [$onoff] \c"
	    read ans
	    [ -z "$ans" ] && ans="$onoff"
	    [ "$ans" = y -o "$ans" = n ] && break
	    echo "Error: you must answer \"y\" or \"n\" ... try again"
	done
	onoff="$ans"
	if [ "$onoff" = y ]
	then
	    echo "Logging interval? [$delta] \c"
	    read ans
	    [ -z "$ans" ] && ans="$delta"
	    delta="$ans"
	fi
    else
	echo ".\c"
    fi

    echo "#+ $tag:$onoff:$delta:" >>$tmp.head
    echo "$desc" | fmt | sed -e 's/^/## /' >>$tmp.head
    if [ "$onoff" = y ]
    then
	if [ ! -z "$metrics" ]
	then
	    echo "log advisory on $delta {" >>$tmp.head
	    for m in $metrics
	    do
		echo "	$m" >>$tmp.head
	    done
	    echo "}" >>$tmp.head
	fi
	if [ ! -z "$metrics_a" ]
	then
	    echo "log advisory on $delta {" >>$tmp.head
	    for m in $metrics_a
	    do
		echo "	$m" >>$tmp.head
	    done
	    echo "}" >>$tmp.head
	fi
    fi
    echo "#----" >>$tmp.head
    cat $tmp.head $tmp.tail >$tmp.ctl

done


if $prompt
then
    echo
    if diff $1 $tmp.ctl >/dev/null
    then
	echo "No changes"
    else
	echo "Differences ..."
	${DIFF-diff} $1 $tmp.ctl
	while true
	do
	    echo "Keep changes? [y] \n"
	    read ans
	    [ -z "$ans" ] && ans=y
	    [ "$ans" = y -o "$ans" = n ] && break
	    echo "Error: you must answer \"y\" or \"n\" ... try again"
	done
	[ "$ans" = y ] && cp $tmp.ctl $1
    fi
else
    echo
    cp $tmp.ctl $1
fi

exit 0
