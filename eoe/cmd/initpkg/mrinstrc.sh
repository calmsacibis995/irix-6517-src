#! /bin/sh
#Tag 0x00000f00
#ident "$Revision $"

. mrprofrc

SHELL=/bin/csh; export SHELL
MRNETRC=/etc/mrnetrc

# miniroot script - invoke inst in the miniroot.
#
# If inst exit's with a non-zero value (not successful),
# then:
#   1) if there is a core, move it to /root/usr/tmp
#   2) explain to the user what happened
#
# In any case, ask the user whether they want to go right back into
# inst, or they want to leave the miniroot and
# boot off the normal root.
#
# The expectation is that inst won't let the user quit
# until /root has a useful system installed on it.  This is handled
# (or at least, user is notified it wasn't true) by sash setting
# the OSLoadOptions nvram variable to begin with "INST", and when
# we complete normally, the mrvolhdrrc script clears removes the
# leading INST, and sets it to start with "inst", so sash can do any
# necessary cleanup (this used to involve changing the volhdr 
# root partition to be the same as swap, but no longer; we could just
# clear the leading "INST", but set to "inst" in case it turns out we want
# more cleanup later on).  sash needs the code anyway to allow the user
# to cleanup if they don't want to (re)load the miniroot.
#
# Anytime inst exits, we let the user leave the miniroot,
# either because inst voluntarily exit'd, and it's ok, or
# because inst croaked, and we should let the user out of
# this mad house if they want.

tapedevice=`nvram tapedevice 2>/dev/null || echo 0`
dlserver=`nvram dlserver 2>/dev/null || echo 0`
instfromopt=
cdromBlockDev=/		# '/' is for sure not a block-device file
tapeRawDev=/		# '/' is for sure not a char-device file
mrmode=`nvram mrmode 2>/dev/null || echo 0`

# Determine if we were invoked as part of an automatic miniroot boot. 
# If so, inst must be invoked in command mode with the provided
# command file.

automr_file=/var/tmp/.auto_mr
hist_file=/root/var/inst/hist
automr_vh=auto_mr
commandfile=
actual_sum=00000
hist_sum=

/sbin/dvhtool -v g $automr_vh $automr_file > /dev/null 2>&1
# We cannot rely on the dvhtool exit status
if [ -s $automr_file ] ; then

   sa=`grep "^SA=" $automr_file | sed s/\^SA=//`
   tape=`echo $tapedevice | sed s/\(mr\)$//`

   # In case the user has changed install sites from the prom

   if [ "$sa" = "$tape" ] ; then
	commandfile=`grep "^CFILE=" $automr_file | sed s/\^CFILE=//`
	# move the command file into a temporary name so if the machine
	# panics or needs to be reset during inst we don't automatically
	# run the command file again
	if [ -r "$commandfile" ] ; then
	   mv -f "$commandfile" "$commandfile".O
	   commandfile="$commandfile".O
	else
	   commandfile=
        fi
       # dlserver is a variable normally set in the prom 
       # that we must set manually
	dlserver=`grep "^dlserver=" $automr_file | sed s/\^dlserver=//`
   fi

   # If user has provided a comparision hist file checksum, make
   # sure actual hist file checksum is the same.  Otherwise,
   # bag command mode.

   hist_sum=`grep "^SUM=" $automr_file | sed s/\^SUM=//`
   if echo "$hist_sum" | grep . > /dev/null ; then
       if [ -r "$hist_file" ] ; then
	  actual_sum=`sum -r $hist_file | awk '{print $1}'`
       fi
       if [ "$hist_sum" != "$actual_sum" ] ; then
          commandfile=
       fi
   fi

fi

## Figure out what we booted from.
##  If used bootp to boot from net, set inst -f option to the
##  server and directory thereon from which we booted.
##  If used local CDROM, set $cdromBlockDev for code that follows,
##  which will mount the CDROM correctly and set inst -f option for it.


## Case 1: booting from net, using bootp

if echo "$tapedevice" | grep '.*bootp()[^ :()]*:[^ :()]*/sa(mr)' >/dev/null
then
    # If tapedevice is read in as	bootp()taylor:/var/boot/sa(mr)
    # then we pass it to inst as	    -f taylor:/var/boot/sa
    # On Indy's (ARCS proms?), tapedevice is preceeded by "network(0)".
    # Strip that as well.

    instfromopt=`echo "$tapedevice" |
		sed 's;.*bootp()\([^ :()]*:[^ :()]*\)/sa(mr);-f\1;'`

    machtype=`uname -m`
    # Copy miniroot unix to /
    location=`echo "$tapedevice" |
	    sed 's;.*\([^ :()]*:[^ :()]*\)/sa(mr);\1/miniroot;'`
    kernel=$dlserver$location/unix.`uname -m`

    # Check if the network it running ... just in case
    $MRNETRC netisup
    if [ $? -ne 0 ]	
    then
	    # Network down ... attempt a restart
	    $MRNETRC forcestart askipaddr
    fi
    tftp <<- EOF >/dev/null 2>&1
	    mode binary
	    get $kernel /unix
	    quit
	EOF
    
    if [ ! -f /unix -o ! -s /unix ]
    then
		# try again, might work this time...
	    tftp <<- EOF >/dev/null 2>&1
		    mode binary
		    get $kernel /unix
		    quit
		EOF
		if [ ! -f /unix -o ! -s /unix ]
		then
			echo "TFTP failure: get $kernel /unix\n\t(tapedevice=$tapedevice)"
		fi
    fi

## Case 2: booting from local CDROM, with ARCS proms

elif echo "$tapedevice" |
	    grep '.*scsi([0-9][0-9]*)cdrom([0-9][0-9]*)partition(8)(mr)' >&-
then
    # On ARCS proms, if boot from CDROM, then tapedevice
    # nicely shows that as: scsi(0)cdrom(7)partition(8)(mr)

    cdromSCSIctlr=`echo "$tapedevice" |
	sed 's;.*scsi(\([0-9][0-9]*\))cdrom([0-9][0-9]*)partition(8)(mr);\1;'`
    cdromSCSIunit=`echo "$tapedevice" |
	sed 's;.*scsi([0-9][0-9]*)cdrom(\([0-9][0-9]*\))partition(8)(mr);\1;'`
    cdromBlockDev=/dev/dsk/dks${cdromSCSIctlr}d${cdromSCSIunit}s7

## Case 3: booting from local tape, with ARCS proms

elif echo "$tapedevice" |
	    grep '.*scsi([0-9][0-9]*)tape([0-9][0-9]*)(mr)' >&-
then
    # On ARCS proms, if boot from Tape, then tapedevice
    # nicely shows that as: scsi(0)tape(7)(mr)

    tapeSCSIctlr=`echo "$tapedevice" |
	sed 's;.*scsi(\([0-9][0-9]*\))tape([0-9][0-9]*)(mr);\1;'`
    tapeSCSIunit=`echo "$tapedevice" |
	sed 's;.*scsi([0-9][0-9]*)tape(\([0-9][0-9]*\))(mr);\1;'`
    tapeRawDev=/dev/rmt/tps${tapeSCSIctlr}d${tapeSCSIunit}nrs

fi


# If booting from CDROM, then we mount the CDROM and pass inst -f/CDROM/dist

if test -b $cdromBlockDev
then

    # If user had CDROM in their fstab, we already mounted it.
    # We have to unmount it, and try to get that entry out of the
    # miniroot /etc/fstab, so that we can mount it on /CDROM
    # instead of on, as is likely, /root/CDROM.  The umount
    # command won't clean up /etc/mtab unless invoked with a
    # block dev or mount pt that matches what it is mounted by.
    # So we'll try that way if we can find the old fstab line.

    cntFstabCDromLines=`egrep -c $cdromBlockDev'|/CDROM' /etc/fstab`
    if test $cntFstabCDromLines -eq 1
    then
	egrep -v $cdromBlockDev'|/CDROM' /etc/fstab > /etc/fstab.NEW
	oldCDromMntDev=`egrep $cdromBlockDev'|/CDROM' /etc/fstab | awk '
							    {print $1}'`
	mv /etc/fstab.NEW /etc/fstab
	( umount $oldCDromMntDev ) 1>&- 2>&-
    fi

    # In any case do our best to see that its unmounted
	
    ( umount $cdromBlockDev ) 1>&- 2>&-

    # Now mkdir /CDROM

    ( rm -f /CDROM; test -d /CDROM || mkdir /CDROM ) 1>&- 2>&-
    instfromopt="-f/CDROM/dist"

    # Copy unix to / (ie. swap) ....
    # First mount /CDROM and then copy unix ...
    machtype=`uname -m`
    echo "$cdromBlockDev /CDROM efs ro,noquota 0 0" >> /etc/fstab
    mount /CDROM
    cp /CDROM/dist/miniroot/unix.$machtype /unix
    umount /CDROM
fi

# If booting from Tape, then set instfromopt accordingly

if test -c $tapeRawDev
then
    instfromopt="-f$tapeRawDev"
fi

# If first argument is "before", then save the current size/checksum
# of the hist file in a signature file.  If the argument is "after", then
# compare the history signature to the previous file.  If the files
# differ, an install has been performed with the inst in this miniroot.
# If 6.2-6.4 is installed, this means /root/usr/sbin/inst won't be
# able to read the hist format on bootup, so downgrade using inst -Z.

chkhistformat()
{
    sigfile=/tmp/hist$1.$$	# histbefore or histafter
    rm -f $sigfile
    if [ -f /root/var/inst/hist ]; then
        /sbin/stat -qs /root/var/inst/hist > $sigfile
	sum -r /root/var/inst/hist | nawk '{print $1'} >> $sigfile
    else
        cat /dev/null > $sigfile
    fi

    if [ "$1" = after -a -f /tmp/histbefore.$$ -a -f /tmp/histafter.$$ ]; then
	cmp /tmp/histbefore.$$ /tmp/histafter.$$ > /dev/null 2>&1 || {

            # Is 6.2-6.4 eoe.sw.unix installed?
	    echo admin hardware | /usr/sbin/inst -r /root -Vverbose:on 2>&1 \
		| nawk -F= -v s=1 \
		    '{ if ($1 == "TARGOS" && match($2,"^[0-9]+$") \
			   && (int($2) < 1270000000)) s=0 }
		     END { exit s  }' || return

	    # Old inst needs old hist format
	    echo "Converting installation history format"
	    /usr/sbin/inst -r /root -Z
	}
    fi
}


# custom mode and commandfile mode should only ever run once between them
firsttime=1

while : reinvoking inst
do
    instcmd=inst
    test -f /usr/sbin/inst || {
	echo Cannot find installation utility /usr/sbin/inst
	instcmd=false
    }

    rm -f core

    # Keep track of whether the hist file changes.  Make a reference
    # file whose timestamp is either now, or the same as the current
    # hist file.
    chkhistformat before

    # Testing hook.  You can shell escape from inst, make a
    # symlink /dbx to a copy of dbx (or gnome?) under /root,
    # and replace the miniroot /usr/sbin/inst with a non-stripped
    # inst from under /root (copy or symlink), then exit
    # the subshell, quit inst, answer 'n' to the restart question,
    # and end back up running the non-stripped inst under dbx.

    if test -f /dbx
    then
	trap 2
	echo
	echo Invoking $instcmd under dbx.
	echo Use dbx command \"run -r /root\" to start $instcmd
	echo
	/dbx /usr/sbin/$instcmd

    elif mrcustomrc iscustom && [ $firsttime -eq 1 ] ; then

	# In custom mode, execute preinst and inst phases
	# otherwise use inst commands from mrconfig and
	# ignore exit codes here

	firsttime=0
	mrcustomrc preinst -r /root "$instfromopt"
	mrcustomrc inst -r /root "$instfromopt"
	/bin/true

    elif [ -n "$commandfile" -a $firsttime -eq 1 ]; then

	   firsttime=0
	   # Only invoke inst in command mode once.  Any subsequent
           # invocation probably indicates trouble so interactive
           # mode is the safest choice
	   $instcmd -r /root -c $commandfile

    elif [ "$instfromopt" ]; then

	$instcmd -r /root "$instfromopt"

    else
	$instcmd -r /root
    fi

    instExitVal=$?

    # inst may have been in graphical mode in which case we need
    # to bring back the textport for possible user interaction
    restoretextport

    # if irix 6.2/6.3/6.4 was installed, downgrade history to
    # an older format.
    chkhistformat after
    
    # Should be "if test $instExitVal -ne 0" but new inst
    # exits non-zero for a variety of trivial reasons.

    if test $instExitVal -gt 128
    then
	# Handle non-successful inst exit values.

	cat <<-\!!
		Installation Termination Error
		
		An error has occurred during your installation session.
		The installation program has terminated abnormally.
	!!

	test $instExitVal -gt 128 && {
	    exitSignal=`expr $instExitVal - 128`
	    case $exitSignal in
	    4|5|6|7|8|10|11|12)		# ILL|TRAP|ABRT|EMT|FPE|BUS|SEGV|SYS)
		test -s core -a -d /root/usr/tmp &&
		    ( file core | grep "dump of '$instcmd'" ) >&- 2>&- &&
		    echo '\nSaving state.  Please wait ...'
		    gzip < core > /root/usr/tmp/$instcmd.core.Z &&
		    egrep Header core |
			sed -n '
			    /.*\(.Header:[ -#%-~]*\$\).*/s//\1/p
			' > /root/usr/tmp/$instcmd.ident &&
		    rm core &&
		    sync &&
		    cat <<-!!

		The $instcmd program dumped core.  A copy of the core
		was saved in /root/usr/tmp/$instcmd.core.Z
			!!
		;;
	    esac
	}

	cat <<-!!

		You may be able to proceed with the installation by reentering $instcmd and
		continuing the installation process.  To do this, answer "n" to the restart
		question below.  If you answer "y" to the restart question, the system will
		attempt to boot the UNIX kernel in /unix.  In either case, your system has
		been left in an unknown state and re-installation of the software you were
		trying to install is recommended.

	!!
    fi



    # run autoconfig and move /root/unix.install to /root/unix,
    # except in custom mode, where this is handled by mrcustomrc
    if mrcustomrc iscustom ; then
	mrcustomrc autoconfig
    else
	mrconfigrc
    fi
    configExitVal=$?

    if [ -d /root/dev/dsk/xlv -o -d /dev/dsk/xlv ] ; then
	#
	# The xlv device directory exists, look for logical volumes.
	#
	mrcnt=`/bin/ls /root/dev/dsk/xlv 2> /dev/null | /sbin/wc -w`
	rcnt=`/bin/ls /dev/dsk/xlv 2> /dev/null | /sbin/wc -w`
	cnt=`expr $rcnt + $mrcnt`
	if [ $cnt -eq 0 ] ; then
	    #
	    # There are no logical volumes so remove the directories.
	    #
	    rm -rf /root/dev/dsk/xlv > /dev/null 2>&1
	    rm -rf /root/dev/rdsk/xlv > /dev/null 2>&1
	else
	    #
	    # There are logical volumes. Verify that the xlv software
	    # is present on this system.
	    #
	    if [ ! -x /root/sbin/xlv_assemble ] ; then
		cat <<-\!!
	WARNING: There appear to be xlv volumes but the xlv software does not
	appear to be installed.  To correct the problem, return to inst by
	answering "n" to the restart question below and then install the xlv
	subsystem.  If you answer "y" to the restart question below, the system
	may not come up properly.
	!!
	    fi
	fi
    fi

    # If inst was invoked as part of an automatic miniroot boot and
    # the install did not core dump and the kernel was built properly
    # exit quietly and attempt to restart the system.
    # If inst is reinvoked, make sure it is in interactive mode

    if echo "$commandfile" | grep . > /dev/null ; then
	commandfile=
	/sbin/dvhtool -v d $automr_vh > /dev/null 2>&1

	if [ $instExitVal -lt 128 -a $configExitVal -eq 0 ] ; then
	    exit 0
        fi
    fi
	    
    # Same for mrmode=custom, except if there is an old kernel
    # might as well attempt a reboot.  Report autoconfig status
    # and run postinst.

    if mrcustomrc iscustom ; then
	mrcustomrc postinst -r /root "$instfromopt"
	if [ $configExitVal -eq 0 -o -s /root/unix ]; then
	    exit 0
	else
	    mrmode=0
        fi
    fi

    while : asking user whether to restart the system
    do
	echo 'Ready to restart the system.  Restart? { (y)es, (n)o, (sh)ell, (h)elp }: \c'
	read line

	case "$line" in
	[yY]*)
	    break 2	# leaves inst reinvoking loop
	    ;;
	[nN]*)
	    echo Reinvoking software installation.
	    break	# reinvoking inst
	    ;;
	sh|SH|shell|SHELL)
	    ( echo set prompt="'miniroot> '" > /.prompt ) > /dev/null
	    csh
	    echo
	    ;;
	shroot)
	    chroot /root /bin/csh
	    echo
	    ;;
	[hH]*)
	    cat <<-\!!
    
		yes - Will leave this miniroot and reboot
		      from your system disk.
		
		no  - Will reinvoke the software installation tool
		      from this miniroot.
		
		sh  - Invoke a subshell (using C Shell csh),
		      in the miniroot, as root.  You can also
		      type "shroot" to chroot csh below /root.
		
		help - show this message
    
	!!
	    ;;
	*)
	    cat <<-\!!
    
		To reboot from your system disk, type "yes" and press ENTER.
		To resume installing software, type "no" and press ENTER.
		Type "help" for additional explanation.

	!!
	esac
    done # asking user whether to restart the system

done # reinvoking inst
