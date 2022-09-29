#! /bin/sh
#Tag 0x00000f00
#ident "$Revision $"

. mrprofrc

# miniroot script - reorganize file systems when entering state 2.
#	    Handles file system layout changes between IRIX 4 and 5.
# Actions:
#	If separate /usr file system
#		make /var a symlink to /usr/var
#	else
#		make /var a new directory
#	Move $USRDIRStoMV from /usr to /var.
#	Move /usr/lib/cron stuff to /var/cron and /etc/cron.d


# See comments below describing $STATE

STATE=/root/.mrreorgrc_tmp_state


# Short circuit case where nothing to do.

if test $# -eq 1				    \
    -a	\(					    \
	    x"$1" = xreorg -a -f /root/.varupdate   \
	-o					    \
	    x"$1" = xcleanup -a ! -d $STATE	    \
	\)
then
	exit
fi


# Move the following from /usr to /var,
# and create symlinks in /usr pointing to /var.

USRDIRStoMV="spool adm mail preserve tmp lib/inst lib/rfindd"

MOUNTDIRS=

# Rotate $LOG to $LOG.O if it has > 10 kbytes.

LOG=/tmp/mrreorgrc.out
> $LOG # be sure it exists to avoid an error message from du

if test 0"`du -s $LOG | awk '{print $1}'`" -gt 20
then
    mv $LOG $LOG.O
fi


# Send stdout, stderr to $LOG
#
# Take stdin from /dev/null, so that we don't
# need a "-f" flag on each cp/mv/ln/rm below.
#
# Save dups of 0,1,2 file descriptors on 3,4,5
# in case we really want to interact.

exec 3<&0 4>&1 5>&2 </dev/null >>$LOG 2>&1


# Use "show" (like echo) to display user messages:

show()
{
    echo "$@"		# into LOG
    echo "$@" 1>&5	# to what was stderr
}

# write to $LOG: current date, command and actions

echo
date
echo $0: $*
set -x

#-------------- useful functions ---------------------

# checkdir name var
#    Test whether name refers to a directory.
#    If it does, increment var: test -d name && var++
#    Symlinks to directories don't count.

checkdir()
{
    test ! -l $1 -a -d $1 && eval $2=`eval expr '$'$2 + 1`
}

# device filename
#    Determine the device (st_dev) representing
#    the file system on which resides "filename".

device()
{
    stat -qd $1 || echo 0
}

# mkunique name uniquename
#	Given filename "name", append the suffix .sav and
#	the smallest non-negative integer suffix N such that
#	"name.savN" does not refer to a file of any sort.
#	Set the shell variable named by the 2nd argument
#	"uniquename" to the resulting unique name "name.savN"
#	Try suffix ".sav" with no number first.

mkunique()
{
    ls -ld $1.sav || {
	eval $2=$1.sav
	return
    }

    n=0
    while ls -ld $1.sav$n
    do
	n=`expr $n + 1`
    done
    eval $2=$1.sav$n
}

# purge filename undoscript
#	Get rid of "filename"by moving it to
#	"filename.savN", for the smallest natural number N
#	such that this name is unique.
#
#	Write the undo mv command to the
#	file "undoscript" named by the 2nd argument.

purge()
{
    mkunique $1 uniquename
    mv $1 $uniquename && echo mv $uniquename $1 > $2
}

# unpurge undoscript
#	Reverse affects of purge.
#	Simply execute the undoscript provided,
#	then remove the undoscript itself.

unpurge()
{
    sh -x $1
    rm $1
}

# Keep track of actions done during reorg, using state
# files kept under STATE=/root/.mrreorgrc_tmp_state.
# These state files will survive reloading the miniroot,
# will not survive re-mkfs'ing /root, and will be cleaned
# out once inst exits.


setstate()
{
    touch $STATE/$1
}

clearstate()
{
    rm $STATE/$1
}

teststate()
{
    test -f $STATE/$1
}

scriptname()
{
    echo $STATE/$1
}

# reverse arg1 arg2 ...
#   Emit (on stdout) arguments in reverse order supplied

reverse()
{
    for i
    do
        revarg="$i $revarg"
    done
    echo "$revarg"
}

dirname()
{
    ans=`/usr/bin/expr "${1:-.}/" : '\(/\)/*[^/]*//*$' `
    if [ -n "$ans" ];then
	    echo $ans
    else
	    ans=`/usr/bin/expr "${1:-.}/" : '\(.*[^/]\)//*[^/][^/]*//*$' `
	    if [ -n "$ans" ];then
		    echo $ans
	    else
		    echo "."
	    fi
    fi
}

false()
{
    test -f /not_a_file..3.2.1.boom
}

true()
{
    :
}


#-------------- main procedure -----------------------

case $1 in
reorg)


    # The file /.varupdate is shipped in IRIX 5 and higher eoe1.sw.unix
    # Don't reorg any file system containing this file.

    test -f /root/.varupdate && exit


    #-------------- Force all fstab mounts ---------------

    # This script can fail if the umount/mount sequence
    # below changes what's mounted.  Quietly avoid that,
    # by doing an extra umount/mount pair.

    umount -kt efs -b /
    mount -ct efs -b /


    #-------------- anything in /usr to move? ------------

    # If there are no directories on the $USRDIRStoMV under /usr,
    # then:
    #	1) If /var has most (at least all but 3) of these
    #	   directories already, restore /.varupdate file,
    #	2) Else show message about not doing reorg.
    #	3) In either case, exit -- no reorg done.

    nusrdirs=0	# How many $USRDIRStoMV found under /usr
    ndirstomv=0	# How many $USRDIRStoMV altogether
    nvardirs=0	# How many $USRDIRStoMV found under /var

    set +x			# suppress noise from following for loop

    for i in $USRDIRStoMV
    do
        checkdir /root/usr/$i nusrdirs
	ndirstomv=`expr $ndirstomv + 1`
	checkdir /root/var/`basename $i` nvardirs
    done

    echo nusrdirs $nusrdirs, ndirstomv $ndirstomv, nvardirs $nvardirs

    set -x			# resume noisy output

    # fuzzy logic:  allow for 1 to 3 (like usr/lib/rfindd)
    #		    items in $USRDIRStoMV list to not exist.
    ndirstomv=`expr $ndirstomv - 3`

    if test $nusrdirs -eq 0 -a $ndirstomv -le $nvardirs
    then
	# reorg already done; someone removed /.varupdate
	# Beware ** following must have leading tabs **
	cat <<-\!! > /root/.varupdate
		Please don't remove this file /.varupdate.
		
		This file is a marker used by the miniroot "mrreorgrc" script to
		identify file systems which have been reorganized from the IRIX 4
		(and before) file system layout to the IRIX 5 layout.
		
		This one time reorganization moves the /usr subdirectories:
		
		    spool adm mail preserve tmp lib/inst lib/rfindd
		
		to /var, where /var will be a symlink to /usr/var in the case
		that this system has a separately mounted /usr file system.
		
		Removing this file /.varupdate will increase slightly the risk
		that the "mrreorgrc" (formerly part of the instdriver script)
		which runs automatically each time the miniroot is loaded to
		use inst, will inadvertently try to move a major /usr or
		/var subdirectory again.
		
		(The contents of this file, the above text, don't matter.
		 All that matters is the presence or absence of this file.)
		
		Silicon Graphics
		March 2, 1994
	!!
	exit	    # leave silently
    fi

    if test $nusrdirs -eq 0
    then
	# typically here after mkfs on clean install; better not to worry user.
	: show Skipping file system reorganization: nothing under /usr to move.
	exit
    fi


    #-------------- be sure /var not mount point  --------

    # Don't reorg if /root and /root/var are separate file systems,
    # or if /root, /root/usr and /root/usr/var are all separate.

    # Must compute these dev* values before umount below, and
    # then use these values in the /var creation logic after umount.

    devroot=`device /root`
    devusr=`device /root/usr`
    devvar=`device /root/var`
    devusrvar=`device /root/usr/var`

    if test -d /root/var -a ! -l /root/var -a $devroot -ne $devvar
    then
	show Skipping file system reorganization: /root/var is mount point.
	exit
    fi

    if test -d /root/usr -a $devroot -ne $devusr -a	    \
	    -d /root/usr/var -a $devusr -ne $devusrvar
    then
	show Skipping file system reorg: /root/usr, /root/usr/var are both mount points.
	exit
    fi

    for i in $USRDIRStoMV
    do
         usrdir=/root/usr/$i
         devusrdir=`device $usrdir`
         if test ! -l $usrdir -a -d $usrdir -a $devusr -ne $devusrdir
	 then
            MOUNTDIRS="$MOUNTDIRS $usrdir"
	 fi 	
    done

    show File system reorganization ... '\c'

    # Begin actually making changes.  Above was read-only.

    #-------------- Create place for state files ---------

    mkdir $STATE
    test -d $STATE || { show Failed - Cannot write /root; exit; }


    #-------------- Create /var or /usr/var---------------

    # If /root/var already a directory (or symlink thereto)
    # on the same file system as /usr, but not to usr itself
    # (so the mv's work) then let it be.  Otherwise, make it so.

    umount -kt efs -b /,/root,/root/usr
    		# must unmount stuff below /usr so mv's, purge's work

    varinode=`stat -iq /root/var || echo 0`
    usrinode=`stat -iq /root/usr || echo 0`

    test -d /root/var -a $devvar -eq $devusr -a $varinode -ne $usrinode || {

	rmdir /root/var && setstate rmdir_var
	purge /root/var `scriptname unpurge_var`
    
	if test $devroot -eq $devusr
	then
	    # easy case: /root and /root/usr same file system
	    #	     Just put /var there too.
    
	    mkdir /root/var && setstate mkdir_var
	else
	    # more work: make /root/usr/var the real /var,
	    #	     and make /root/var a symlink to usr/var.
    
	    purge /root/usr/var `scriptname unpurge_usr_var`
	    mkdir /root/usr/var && setstate mkdir_usr_var
    
	    ln -s usr/var /root/var && setstate mksymlink_var
	fi
    }

    # final check that mv's from /root/usr to /root/var work:

    for i in 0 1 2 3 4 5 6 7 8 9
    do
	f=/root/usr/.mrreorgrc.$i
	g=/root/var/.mrreorgrc.$i

	{ stat -qd $f || stat -qd $g; } && continue

	mkdir $f
	mv $f $g && rmdir $g || {
	    rmdir $f
	    show Failed - Cannot mv from /root/usr to /root/var
	    # Don't cleanup above /var, /usr/var creation.
    	    # Leave failure evidence around for analysis.
	    # Aren't supposed to be here anyway, because above
	    # checks for /var, et al mount points should be
	    # sufficient to ensure that such mv's "usually" work.
	    exit
	}
	break
    done


    #-------------- mv /usr/$USRDIRStoMV to /var ---------

    # Move /usr subdirectories:
    #	USRDIRStoMV="spool adm mail preserve tmp lib/inst lib/rfindd"
    # to /var.

    for i in $USRDIRStoMV
    do
	echo
	usr_name=/root/usr/$i
	base=`basename $i`
	var_name=/root/var/$base

	# example sym_prefix's (relative symlink prefix):
	#   usr_name		     sym_prefix
	#   /root/usr/adm		..
	#   /root/usr/tmp		..
	#   /root/usr/lib/inst		../..
	#   /root/usr/lib/rfindd	../..
	sym_prefix=`echo $usr_name |
			sed -e 's;/root/usr/;;' -e 's;[^/][^/]*;..;g'`

	purge $var_name `scriptname unpurge_var.$base`

	# Determine if $usr_name is a symlink that points (perhaps
	# multi-hop) at $var_name.  Do so by temporarily placing a
	# directory at $var_name, seeing if $usr_name stat's as a
	# directory, removing the $var_name directory, and seeing
	# if $usr_name no longer stat's as a directory.

	mkdir $var_name
	usrWasDir=`test -d $usr_name && echo true || echo false`
	rmdir $var_name

	if `$usrWasDir` && test ! -d $usr_name
	then
	    # $usr_name -> $var_name.  Desired reorg action: noop.
	    unpurge `scriptname unpurge_var.$base`
        elif echo "$MOUNTDIRS" | grep "$usr_name" > /dev/null  
	then
	    show "\nSkipping relocation of $usr_name since it is at or below a mount point."
	    unpurge `scriptname unpurge_var.$base`
	else
	    mv $usr_name /root/var &&
		setstate mv.$base
	    ln -s $sym_prefix/var/$base $usr_name &&
		setstate ln.$base
	fi
    done

    echo
    mount -ct efs -b /,/root,/root/usr
    		# remount any further stuff below /usr
    echo


    #-------------- move /usr/lib/cron -------------------

    # If /usr/lib/cron exists, then
    #	move its log files to /var/cron
    #	move other files to /etc/cron.d

    if test -d /root/usr/lib/cron
    then
	cd /root/usr/lib/cron

	purge /root/var/cron `scriptname unpurge_var_cron`
	mkdir /root/var/cron && setstate mkdir_var_cron
	test -d /root/var/cron &&
	    mv *log /root/var/cron &&
	    setstate mv.var_cron
    
	purge /root/etc/cron.d `scriptname unpurge_etc_cron.d`
	mkdir /root/etc/cron.d && setstate mkdir_etc_cron.d
	test -d /root/etc/cron.d &&
	    mv `ls -A1` /root/etc/cron.d &&
	    setstate mv.etc_cron.d

	cd /

    fi

    rmdir /root/usr/lib/cron && setstate rmdir_usr_lib_cron
    purge /root/usr/lib/cron `scriptname unpurge_usr_lib_cron`


    #-------------- done with reorg ----------------------

    ls -lt $STATE

    # simulatation of: head $STATE/*
    for i in $STATE/*
    do
	: '==> '$i' <=='
	sed 10q $i
	:
    done

    d=/root/var/inst
    while :
    do
	test -d $d && break
	d=`dirname $d`
    done
    gzip -c $LOG > $d/.mrreorgrc.debug.Z

    show done

    ;;


cleanup)

    test -d $STATE || exit	    # exit if no reorg in progress
    
    if test -f /root/.varupdate
    then
    
	#-------------- cleanup ------------------------------
    
	# We just upgraded to IRIX 5.  Cleanup the saved files
	# that were left around in case we had to undo the reorg.
	# The old instdriver reorg code only did one thing here:
	# remove /root/var.sav if it had less than 20 files below it.
	# Why risking removing a small var.sav?  Let's leave it,
	# and do nothing (other than clean out $STATE, below).

	: noop

    else

	#-------------- undo ---------------------------------
    
	# Apparently didn't upgrade to IRIX 5 after all.
	# Undo the reorg changes done above, in reverse order.


	#-------------- Force all fstab mounts ---------------
    
	# This script can fail if the umount/mount sequence
	# below changes what's mounted.  Quietly avoid that,
	# by doing an extra umount/mount pair.
    
	umount -kt efs -b /
	mount -ct efs -b /

	unpurge `scriptname unpurge_usr_lib_cron`

	teststate rmdir_usr_lib_cron &&
	    mkdir /root/usr/lib/cron &&
	    clearstate rmdir_usr_lib_cron

	if test -d /root/usr/lib/cron
	then
	    teststate mv.etc_cron.d &&
		test -d /root/etc/cron.d && {
		    cd /root/etc/cron.d
		    mv `ls -A1` /root/usr/lib/cron
		    cd /
		} &&
		clearstate mv.etc_cron.d
    
	    teststate mkdir_etc_cron.d &&
		rmdir /root/etc/cron.d &&
		clearstate mkdir_etc_cron.d
    
	    unpurge `scriptname unpurge_etc_cron.d`

	    teststate mv.var_cron &&
		test -d /root/var/cron && {
		    cd /root/var/cron
		    mv *log /root/usr/lib/cron
		    cd /
		} &&
		clearstate mv.var_cron
    
	    teststate mkdir_var_cron &&
		rmdir /root/var/cron &&
		clearstate mkdir_var_cron
    
	    unpurge `scriptname unpurge_var_cron`
	fi

	umount -kt efs -b /,/root,/root/usr
		# must unmount stuff below /usr so mv's works

	for i in `reverse $USRDIRStoMV`
	do
	    echo
	    usr_name=/root/usr/$i
	    base=`basename $i`
	    var_name=/root/var/$base

	    teststate ln.$base &&
		test -l $usr_name &&
		rm $usr_name &&
		clearstate ln.$base
	    teststate mv.$base &&
		test -d `dirname $usr_name` &&
		test ! -d $usr_name &&
		mv $var_name $usr_name &&
		clearstate mv.$base
	    unpurge `scriptname unpurge_var.$base`
	done

	mount -ct efs -b /,/root,/root/usr
		# remount any further stuff below /usr

	teststate mksymlink_var &&
	    test -l /root/var &&
	    rm /root/var &&
	    clearstate mksymlink_var

	teststate mkdir_usr_var &&
	    rmdir /root/usr/var &&
	    clearstate mkdir_usr_var

	unpurge `scriptname unpurge_usr_var`

	teststate mkdir_var &&
	    rmdir /root/var &&
	    clearstate mkdir_var

	unpurge `scriptname unpurge_var`

	teststate rmdir_var &&
	    mkdir /root/var &&
	    clearstate rmdir_var

    fi

    ls -lt $STATE
    # simulatation of: head $STATE/*
    for i in $STATE/*
    do
	: '==> '$i' <=='
	sed 10q $i
	:
    done
    rm -r $STATE

    d=/root/var/inst
    while :
    do
	test -d $d && break
	d=`dirname $d`
    done
    gzip -c $LOG > $d/.mrreorgrc.debug.Z

    ;;

*)
    show Usage: mrreorgrc '{reorg|cleanup}'
    ;;

esac
