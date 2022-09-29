#! /bin/sh
#Tag 0x00000f00
#ident "$Revision $"


# Reconfigure kernel under /root.
# Invoked from mrinstrc, after inst exit successfully.
# If there is a /root/unix.install, move it to /root/unix

. mrprofrc

status=0

unix=/root/unix
unixS=/unix
unixSZ=/unix.gz
unixZ=/unix.root.Z
unixI=/root/unix.install

#
# Compress the kernel in /unix ... not the /root ...
# at least no yet.  This should allow the safety
# kernel (below) to be saved without increasing
# the amount of swap space for miniroot.
#
if test -f $unixS -a -s $unixS 
then
	gzip $unixS || rm -rf $unixSZ
fi

# If the autoconfig script isn't present or if it reports
# there is no work to do, leave quietly. Likely, they
# haven't installed much yet.
# However, first check to see if the /root/unix has a date of
# 1 Jan 2000, which could happen if an old-style "0101000000"
# was run as part of a removeop on a patch, for example.
#
test -f /root/etc/init.d/autoconfig || exit

if [ "`stat -qm /root/unix`" -eq 946713600 ] ; then
	touch -c -t 197001010000 /root/unix
fi

chroot /root /etc/init.d/autoconfig -n -o -N >/dev/null 2>&1 && exit

echo Automatically reconfiguring the operating system.

# Compress /root/unix into /unix.root.Z and zero-out /root/unix
# we preserve acc/mod times for autoconfig and/or in case of
# failures
#
test -f $unix -a -s $unix && {
	gzip -c $unix > $unixZ || rm -f $unixZ
	if [ -s $unixZ ]
	then
		touch -r $unix $unixZ
		rm -f $unix
		touch -r $unixZ $unix
	fi
	# else the gzip failed, and we don't want to create a /root
	# unix with a modtime of right now, because that turns autoconfig
	# into a nop.
}

# Reconfigure the kernel
#
if echo y | chroot /root /etc/init.d/autoconfig -o -N >/dev/null
then
    if test -f $unixI -a -s $unixI
    then
        mv -f $unixI $unix
    else
	# we should never get here because we just made sure via
	# autoconfig -n that a new kernel would get configured.
	# just in case though...
	if test -f $unixZ
	then
            gunzip -c $unixZ > $unix
	    touch -r $unixZ $unix
	fi
    fi
else
    cat <<-\!!
	ERROR IN KERNEL CONFIGURATION
	
	The UNIX kernel is not properly installed on the system, probably as 
	a result of an autoconfig(1M) failure.
	
	To correct the problem, return to inst by answering "n" to the restart 
	question below, then give the command "help kernelerror" to get more
	information that will help you diagnose and solve the problem.
	
	If you answer "y" to the restart question below, the system may not come
	up properly, or the system may come up running an old kernel that might
	be incompatible with the new software you just installed.

	!!
	status=1
fi


# Cleanup
#
if test -f $unixZ -a \( ! -f $unix -o ! -s $unix \)
then
    gunzip -c $unixZ > $unix
    touch -r $unixZ $unix
fi

rm -f $unixZ > /dev/null

#
# Wait for $unixZ to be removed before
# trying to gunzip the /unix.  Just incase
# we need to retry miniroot install again.
#
if test -f $unixSZ -a -s $unixSZ
then
	gunzip $unixSZ
fi

rm -f $unixSZ > /dev/null

# mrinstrc needs to know if autoconfig failed
exit $status
